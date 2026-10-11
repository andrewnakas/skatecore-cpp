// Port of skate-game/src/physics/offboard/settings/metrics.rs
#include "s3/game/physics/offboard/settings/metrics.hpp"

#include <cctype>

namespace s3::game::physics::offboard::settings::metrics {

namespace {
bool eq_ignore_ascii_case(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const unsigned char x = static_cast<unsigned char>(a[i]);
        const unsigned char y = static_cast<unsigned char>(b[i]);
        const unsigned char lx = (x >= 'A' && x <= 'Z') ? static_cast<unsigned char>(x + 32) : x;
        const unsigned char ly = (y >= 'A' && y <= 'Z') ? static_cast<unsigned char>(y + 32) : y;
        if (lx != ly) return false;
    }
    return true;
}
}  // namespace

rs::Result<std::array<std::optional<ClipMetric>, 3>, std::string> load(const sd::AnimationMetadata& metadata) {
    auto one = [&](const std::string& name) -> rs::Result<std::optional<ClipMetric>, std::string> {
        S3G_TRY(const sd::ClipMetadata* clip, metadata.clip(name));
        const auto* source = metadata.source_for(name);
        if (source == nullptr) return rs::make_err(fmt::cat("Missing bank identity for ", name));
        if (!eq_ignore_ascii_case(source->source_bank, "OffBoard.abin")) {
            return rs::make_err(fmt::cat(name, ": expected actual OffBoard.abin source"));
        }
        return metric(*clip);
    };
    std::array<std::optional<ClipMetric>, 3> out{};
    S3G_TRY(out[0], one(CLIPS[0]));
    S3G_TRY(out[1], one(CLIPS[1]));
    S3G_TRY(out[2], one(CLIPS[2]));
    return out;
}

rs::Result<std::optional<ClipMetric>, std::string> metric(const sd::ClipMetadata& clip) {
    const sd::ClipAttribute* attribute = nullptr;
    for (const auto& a : clip.attributes) {
        if (a.name == "ANIMTRANSZ") {
            attribute = &a;
            break;
        }
    }
    if (attribute == nullptr) return std::optional<ClipMetric>{};
    // All three actual stock records are scalar. Reject unsupported encodings
    // instead of reading curve metadata or an unwritten native payload as a value.
    if (attribute->type_id != 0) {
        return rs::make_err(fmt::cat(clip.name, ": AnimTransZ expected scalar stock record"));
    }
    if (attribute->payload_words.empty()) return rs::make_err(fmt::cat(clip.name, ": truncated AnimTransZ"));
    const float translation_z = rs::from_bits(attribute->payload_words.front());
    const float length = rs::from_bits(clip.frames_bits) / rs::from_bits(clip.fps_bits);
    const float end_time = rs::from_bits(attribute->end_bits) * length;
    if (!rs::is_finite(translation_z) || !rs::is_finite(end_time) || end_time == 0.0f) {
        return rs::make_err(fmt::cat(clip.name, ": invalid AnimTransZ metric"));
    }
    return std::optional<ClipMetric>(ClipMetric{translation_z, end_time});
}

}  // namespace s3::game::physics::offboard::settings::metrics
