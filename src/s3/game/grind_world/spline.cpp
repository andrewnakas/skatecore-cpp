// Port of skate-game/src/grind_world/spline.rs.
//! User-authorized ZIP spline adapter, checked against original TU3 82C1E568,
//! 82C1E6C0, 82C1EEF0 and 82C1F098 in default.patched.xex (431b8eba...).
//! Keep the native cubic payload; the game's contact primitive is its chord
//! D -> (A+B)+(C+D), including for retail cubic segments.
#include "s3/game/grind_world/spline.hpp"

#include <cmath>
#include <limits>

#include "s3/game/common.hpp"

namespace s3::game::grind_world::spline {

rs::Result<std::vector<std::uint8_t>, std::string> build(const SkateMap* map) {
    static const std::vector<Rail> empty;
    return build_rails(map != nullptr ? map->rails : empty);
}

rs::Result<std::vector<std::uint8_t>, std::string> build_rails(const std::vector<Rail>& rails) {
    // TU3 82C1EEF0 reads the rail count with lhz +2, not a full 32-bit load.
    if (rails.size() > std::size_t(0xFFFF)) {
        return rs::make_err(std::string("Pegasus spline table exceeds its uint16 rail count"));
    }
    using Segment = std::array<std::uint32_t, 30>;
    std::vector<std::pair<std::array<std::uint32_t, 6>, std::vector<Segment>>> records;
    for (const Rail& rail : rails) {
        std::array<std::uint32_t, 6> header{};
        std::vector<Segment> segments;
        if (rail.native.has_value()) {
            const std::vector<std::uint8_t>& raw = *rail.native;
            if (raw.size() < 28) {
                return rs::make_err(fmt::cat("Truncated native rail ", rail.name));
            }
            // .skate is little endian; Pegasus blobs are big endian.
            std::vector<std::uint32_t> words;
            words.reserve(raw.size() / 4);
            for (std::size_t i = 0; i + 4 <= raw.size(); i += 4) {
                words.push_back(std::uint32_t(raw[i]) | (std::uint32_t(raw[i + 1]) << 8) |
                                (std::uint32_t(raw[i + 2]) << 16) | (std::uint32_t(raw[i + 3]) << 24));
            }
            for (std::size_t i = 0; i < 6; ++i) header[i] = words[i];
            // u64 fields use low/high words in the package.
            std::swap(header[0], header[1]);
            std::swap(header[2], header[3]);
            if (std::size_t(words[6]) != (raw.size() - 28) / 120 || (raw.size() - 28) % 120 != 0) {
                return rs::make_err(fmt::cat("Invalid native rail segment count: ", rail.name));
            }
            for (std::size_t at = 7; at + 30 <= words.size(); at += 30) {
                Segment segment{};
                for (std::size_t j = 0; j < 30; ++j) segment[j] = words[at + j];
                segments.push_back(segment);
            }
        } else {
            if (rail.points.size() < 2) {
                return rs::make_err(fmt::cat("Rail ", rail.name, " needs two points"));
            }
            std::uint64_t id = 1469598103934665603ULL;
            auto mix = [&](std::uint8_t byte) { id = (id ^ std::uint64_t(byte)) * 1099511628211ULL; };
            for (const char c : rail.name) mix(std::uint8_t(c));
            for (const auto& p : rail.points) {
                for (const float f : p) {
                    for (const std::uint8_t b : rs::to_be_bytes(f)) mix(b);
                }
            }
            header = {std::uint32_t(id >> 32), std::uint32_t(id), 0x2c701707u, 0x0007004au, 0u, 0u};
            std::vector<std::array<float, 3>> points = rail.points;
            const std::array<float, 3> first = points[0];
            const std::array<float, 3> last = points.back();
            // Closed authored polylines retain their final edge, even when short.
            if (rail.closed && first != last) {
                points.push_back(first);
            }
            for (std::size_t k = 0; k + 1 < points.size(); ++k) {
                const std::array<float, 3> start = points[k];
                const std::array<float, 3> end = points[k + 1];
                Segment segment{};
                for (std::size_t i = 0; i < 3; ++i) {
                    const float delta = end[i] - start[i];
                    segment[i] = rs::to_bits(-2.0f * delta);
                    segment[4 + i] = rs::to_bits(3.0f * delta);
                    segment[12 + i] = rs::to_bits(start[i]);
                    segment[20 + i] = rs::to_bits(rs::min(start[i], end[i]));
                    segment[24 + i] = rs::to_bits(rs::max(start[i], end[i]));
                }
                segment[15] = rs::to_bits(1.0f);
                segments.push_back(segment);
            }
        }
        if (segments.empty()) {
            return rs::make_err(fmt::cat("Rail ", rail.name, " has no segments"));
        }
        for (const Segment& s : segments) {
            for (std::size_t w = 0; w < 28; ++w) {
                if (!std::isfinite(rs::from_bits(s[w]))) {
                    return rs::make_err(fmt::cat("Non-finite spline data in ", rail.name));
                }
            }
            for (std::size_t i = 0; i < 3; ++i) {
                if (rs::from_bits(s[20 + i]) > rs::from_bits(s[24 + i])) {
                    return rs::make_err(fmt::cat("Inverted spline bounds in ", rail.name));
                }
            }
        }
        records.emplace_back(header, std::move(segments));
    }
    std::size_t count = 0;
    for (const auto& r : records) {
        if (__builtin_add_overflow(count, r.second.size(), &count)) {
            return rs::make_err(std::string("Spline segment count overflow"));
        }
    }
    const std::size_t base = 16 + records.size() * 32;
    std::size_t size = 0;
    if (__builtin_mul_overflow(count, std::size_t(144), &size) || __builtin_add_overflow(size, base, &size) ||
        size > std::size_t(std::numeric_limits<std::uint32_t>::max())) {
        return rs::make_err(std::string("Spline blob exceeds uint32 offsets"));
    }
    std::vector<std::uint8_t> bytes(size, 0);
    put(bytes, 0, std::uint32_t(records.size()));
    put(bytes, 4, std::uint32_t(count));
    put(bytes, 8, 16);
    put(bytes, 12, std::uint32_t(base));
    std::size_t at = base;
    for (std::size_t index = 0; index < records.size(); ++index) {
        const auto& header = records[index].first;
        const auto& segments = records[index].second;
        const std::size_t r = 16 + index * 32;
        for (std::size_t i = 0; i < 5; ++i) {
            put(bytes, r + i * 4, header[i]);
        }
        put(bytes, r + 20, std::uint32_t(at));
        put(bytes, r + 24, std::uint32_t(at + (segments.size() - 1) * 144));
        put(bytes, r + 28, header[5]);
        for (std::size_t i = 0; i < segments.size(); ++i) {
            for (std::size_t j = 0; j < 30; ++j) {
                put(bytes, at + j * 4, segments[i][j]);
            }
            put(bytes, at + 120, std::uint32_t(r));
            if (i > 0) {
                put(bytes, at + 124, std::uint32_t(at - 144));
            }
            if (i + 1 < segments.size()) {
                put(bytes, at + 128, std::uint32_t(at + 144));
            }
            at += 144;
        }
    }
    return bytes;
}

rs::Result<std::vector<Primitive>, std::string> primitives(const SkateMap* map) {
    S3G_TRY(auto bytes, build(map));
    return primitives_from_blob(bytes);
}

rs::Result<std::vector<Primitive>, std::string> primitives_from_blob(const std::vector<std::uint8_t>& bytes) {
    S3G_TRY(auto decoded, decoded_from_blob(bytes));
    return std::move(decoded.first);
}

rs::Result<std::pair<std::vector<Primitive>, std::vector<PrimitiveMetadata>>, std::string> decoded_from_blob(
    const std::vector<std::uint8_t>& bytes) {
    auto word = [&](std::size_t at) -> std::uint32_t {
        if (at + 4 > bytes.size()) S3_PANIC("range end index out of range for slice");
        return (std::uint32_t(bytes[at]) << 24) | (std::uint32_t(bytes[at + 1]) << 16) |
               (std::uint32_t(bytes[at + 2]) << 8) | std::uint32_t(bytes[at + 3]);
    };
    std::vector<Primitive> result;
    std::vector<PrimitiveMetadata> metadata;
    const std::size_t base = std::size_t(word(12));
    const std::size_t n = std::size_t(word(4));
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t s = base + i * 144;
        const std::size_t r = std::size_t(word(s + 120));
        std::array<std::array<float, 4>, 4> coefficients{};
        for (std::size_t v = 0; v < 4; ++v) {
            for (std::size_t j = 0; j < 4; ++j) coefficients[v][j] = rs::from_bits(word(s + v * 16 + j * 4));
        }
        const auto& a = coefficients[0];
        const auto& b = coefficients[1];
        const auto& c = coefficients[2];
        const auto& d = coefficients[3];
        std::array<float, 4> end{};
        for (std::size_t j = 0; j < 4; ++j) end[j] = (a[j] + b[j]) + (c[j] + d[j]);
        float length = -0.0f;
        for (std::size_t j = 0; j < 3; ++j) length += rs::powi(end[j] - d[j], 2);
        if (!std::isfinite(length)) {
            return rs::make_err(fmt::cat("Spline segment ", i, " has a non-finite contact chord"));
        }
        // Original 82C1E568 / 82C1F098 emit every segment. Do not drop tiny or
        // duplicate knots here: downstream contact/selection owns admission.
        const float horizontal_tolerance = rs::from_bits(0x37800000u);
        const bool vertical =
            std::fabs(end[0] - d[0]) <= horizontal_tolerance && std::fabs(end[2] - d[2]) <= horizontal_tolerance;
        result.push_back(Primitive{d, end, std::uint64_t((r - 16) / 32 + 1)});
        PrimitiveMetadata m;
        m.spline_guids = {(std::uint64_t(word(r)) << 32) | std::uint64_t(word(r + 4)),
                          (std::uint64_t(word(r + 8)) << 32) | std::uint64_t(word(r + 12))};
        m.segment_index = std::uint32_t((s - std::size_t(word(r + 20))) / 144);
        m.flags = vertical ? 0x80000000u : 0u;
        metadata.push_back(m);
    }
    return std::make_pair(std::move(result), std::move(metadata));
}

void put(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t w) {
    const auto b = rs::to_be_bytes_u32(w);
    for (std::size_t i = 0; i < 4; ++i) bytes[at + i] = b[i];
}

}  // namespace s3::game::grind_world::spline
