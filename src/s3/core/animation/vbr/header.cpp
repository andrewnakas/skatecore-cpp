// Port of skate-core/src/animation/vbr/header.rs.
#include "s3/core/animation/vbr/header.hpp"

#include <cstdio>

namespace s3::animation::vbr::header {

#define S3_VBR_TRY(var, expr)                                                     \
    auto var##_result = (expr);                                                   \
    if (var##_result.is_err()) return rs::make_err(var##_result.unwrap_err());    \
    auto var = var##_result.unwrap()

rs::Result<Header, std::string> Header::read(Slice<const std::uint8_t> bytes, std::size_t part,
                                             std::size_t header_relative, std::size_t data_relative) {
    S3_VBR_TRY(header_word, u32_at(bytes, part));
    const std::size_t bones = static_cast<std::size_t>((header_word >> 6) & 63u) + 1;
    S3_VBR_TRY(header, offset(part, header_relative));
    S3_VBR_TRY(header_end, offset(header, static_cast<std::size_t>(header_word >> 12)));
    S3_VBR_TRY(data, offset(part, data_relative));
    S3_VBR_TRY(size_at, offset(part, 4));
    S3_VBR_TRY(data_size, u32_at(bytes, size_at));
    S3_VBR_TRY(data_end, offset(data, static_cast<std::size_t>(data_size)));
    if (header_end < header) return rs::make_err(std::string("VBR header range"));
    {
        S3_VBR_TRY(unused, take(bytes, header, header_end - header));
        (void)unused;
    }
    if (data_end < data) return rs::make_err(std::string("VBR data range"));
    {
        S3_VBR_TRY(unused, take(bytes, data, data_end - data));
        (void)unused;
    }
    S3_VBR_TRY(frames16, u16_at(bytes, header + 2));
    const std::size_t frames = frames16;
    S3_VBR_TRY(count, take(bytes, header + 12, 2));
    if (count[0] != 3 || frames == 0) {
        return rs::make_err(std::string("VBR SQT requires three channel types and a nonempty frame range"));
    }
    const std::size_t dictionary = header + 16;
    const std::size_t dictionary_count = count[1];
    S3_VBR_TRY(size_table, offset(dictionary, dictionary_count * 4));
    const std::size_t block_count = (frames + 7) / 8;
    S3_VBR_TRY(after_sizes, offset(size_table, block_count * 2));
    S3_VBR_TRY(types_unaligned, offset(after_sizes, 15));
    const std::size_t types = types_unaligned & ~std::size_t(15);
    S3_VBR_TRY(types_end, offset(types, 48));
    if (types_end > header_end) {
        return rs::make_err(std::string("VBR channel descriptions exceed compression header"));
    }
    std::vector<Channel> channels;
    channels.reserve(3);
    const std::size_t expected[3] = {3, 4, 3};
    for (std::size_t i = 0; i < 3; ++i) {
        const std::size_t expected_components = expected[i];
        const std::size_t at = types + 16 * i;
        S3_VBR_TRY(dynamic16, u16_at(bytes, at));
        S3_VBR_TRY(constant16, u16_at(bytes, at + 2));
        S3_VBR_TRY(components_bytes, take(bytes, at + 12, 1));
        S3_VBR_TRY(min_bits, u32_at(bytes, at + 4));
        S3_VBR_TRY(max_bits, u32_at(bytes, at + 8));
        const std::size_t dynamic = dynamic16;
        const std::size_t constant = constant16;
        const std::size_t components = components_bytes[0];
        const float min = rs::from_bits(min_bits);
        const float max = rs::from_bits(max_bits);
        if (components != expected_components || dynamic + constant != bones) {
            return rs::make_err("VBR channel " + std::to_string(i) + " does not match its SQT output descriptor");
        }
        channels.push_back(Channel{dynamic, constant, components, min, max - min});
    }
    std::size_t cursor = data;
    std::vector<float> constants;
    for (const Channel& channel : channels) {
        for (std::size_t k = 0; k < channel.constant * channel.components; ++k) {
            S3_VBR_TRY(index_bytes, take(bytes, cursor, 1));
            const std::size_t index = index_bytes[0];
            cursor += 1;
            if (index >= dictionary_count) {
                return rs::make_err(std::string("VBR constant dictionary index out of bounds"));
            }
            S3_VBR_TRY(value_bits, u32_at(bytes, dictionary + index * 4));
            const float value = rs::from_bits(value_bits);
            // Original scalar fmadds82E8A804..878, including constants.
            constants.push_back(rs::mul_add(value, channel.range, channel.min));
        }
    }
    S3_VBR_TRY(runs16, u16_at(bytes, header));
    const std::size_t runs = runs16;
    std::vector<bool> constant_flags;
    constant_flags.reserve(3 * bones);
    for (std::size_t run = 0; run < runs; ++run) {
        S3_VBR_TRY(length_bytes, take(bytes, cursor, 1));
        const std::size_t length = length_bytes[0];
        cursor += 1;
        if (constant_flags.size() + length > 3 * bones) {
            return rs::make_err(std::string("VBR constant-channel run exceeds channel count"));
        }
        constant_flags.insert(constant_flags.end(), length, run % 2 == 1);
    }
    if (runs == 0 && constants.empty()) {
        constant_flags.resize(3 * bones, false);
    }
    if (constant_flags.size() != 3 * bones) {
        return rs::make_err(std::string("VBR constant-channel runs do not cover all channels"));
    }
    for (std::size_t i = 0; i < channels.size(); ++i) {
        std::size_t set = 0;
        for (std::size_t b = i * bones; b < (i + 1) * bones; ++b) set += constant_flags[b] ? 1 : 0;
        if (set != channels[i].constant) {
            return rs::make_err("VBR channel " + std::to_string(i) + " constant count differs from its runs");
        }
    }
    std::size_t dynamic_count = 0;
    for (const Channel& c : channels) dynamic_count += c.dynamic * c.components;
    std::vector<std::uint32_t> widths;
    widths.reserve(dynamic_count);
    for (std::size_t k = 0; k < dynamic_count; ++k) {
        S3_VBR_TRY(width, u32_at(bytes, cursor));
        widths.push_back(width);
        cursor += 4;
    }
    const std::size_t block_start = cursor;
    std::vector<ByteRange> blocks;
    blocks.reserve(block_count);
    // Vector_ExtractPackedNVBR82D1E0E4..118 accumulates u16 block sizes.
    std::uint16_t displacement = 0;
    for (std::size_t block = 0; block < block_count; ++block) {
        S3_VBR_TRY(size, u16_at(bytes, size_table + 2 * block));
        S3_VBR_TRY(start, offset(block_start, static_cast<std::size_t>(displacement)));
        S3_VBR_TRY(end, offset(start, static_cast<std::size_t>(size)));
        if (end > data_end || end < start || size < 3) {
            return rs::make_err("VBR block " + std::to_string(block) + " exceeds compressed data");
        }
        blocks.push_back(ByteRange{start, end});
        displacement = static_cast<std::uint16_t>(displacement + size);
    }
    S3_VBR_TRY(low_bits, u32_at(bytes, header + 4));
    S3_VBR_TRY(high_bits, u32_at(bytes, header + 8));
    const float low = rs::abs(rs::from_bits(low_bits));
    const float high = rs::abs(rs::from_bits(high_bits));
    Header out;
    out.frames = frames;
    out.bones = bones;
    out.dct_scale = rs::max(low, high);
    out.channels = std::move(channels);
    out.constants = std::move(constants);
    out.constant_flags = std::move(constant_flags);
    out.widths = std::move(widths);
    out.blocks = std::move(blocks);
    return out;
}

#undef S3_VBR_TRY

rs::Result<std::size_t, std::string> offset(std::size_t base, std::size_t relative) {
    if (base > SIZE_MAX - relative) return rs::make_err(std::string("VBR offset overflow"));
    return base + relative;
}

rs::Result<Slice<const std::uint8_t>, std::string> take(Slice<const std::uint8_t> bytes, std::size_t at,
                                                        std::size_t count) {
    auto end = offset(at, count);
    if (end.is_err()) return rs::make_err(end.unwrap_err());
    if (at > end.unwrap() || end.unwrap() > bytes.size()) {
        char buffer[64];
        std::snprintf(buffer, sizeof buffer, "%#zx", at);
        if (at == 0) std::snprintf(buffer, sizeof buffer, "0x0");
        return rs::make_err("VBR read " + std::string(buffer) + "+" + std::to_string(count) + " out of bounds");
    }
    return bytes.sub(at, end.unwrap());
}

rs::Result<std::uint16_t, std::string> u16_at(Slice<const std::uint8_t> bytes, std::size_t at) {
    auto b = take(bytes, at, 2);
    if (b.is_err()) return rs::make_err(b.unwrap_err());
    const auto s = b.unwrap();
    return static_cast<std::uint16_t>((static_cast<std::uint32_t>(s[0]) << 8) | s[1]);
}

rs::Result<std::uint32_t, std::string> u32_at(Slice<const std::uint8_t> bytes, std::size_t at) {
    auto b = take(bytes, at, 4);
    if (b.is_err()) return rs::make_err(b.unwrap_err());
    const auto s = b.unwrap();
    return (static_cast<std::uint32_t>(s[0]) << 24) | (static_cast<std::uint32_t>(s[1]) << 16) |
           (static_cast<std::uint32_t>(s[2]) << 8) | static_cast<std::uint32_t>(s[3]);
}

}  // namespace s3::animation::vbr::header
