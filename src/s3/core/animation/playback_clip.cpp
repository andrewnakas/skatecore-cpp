// Port of skate-core/src/animation/playback_clip.rs.
#include "s3/core/animation/playback_clip.hpp"

#include <utility>

namespace s3::animation::playback_clip {

PlaybackClip PlaybackClip::new_(float frames, float fps, float base_speed, std::uint32_t flags,
                                std::vector<ClipAttribute> attributes) {
    PlaybackClip clip;
    clip.clock.frames = frames;
    clip.clock.fps = fps;
    clip.clock.base_speed = base_speed;
    clip.clock.speed = 1.0f;
    clip.clock.length = (frames - 1.0f) / ((1.0f * base_speed) * fps);
    clip.clock.time = 0.0f;
    clip.clock.previous_time = 0.0f;
    clip.clock.loops_since_evaluation = 0;
    clip.clock.looping = (flags & 0x10000000u) != 0;
    clip.clock.phase_controlled = (flags & 0x40000000u) != 0;
    clip.attributes_ = std::move(attributes);
    return clip;
}

AdvanceResult PlaybackClip::advance(float dt, float phase) {
    AdvanceResult result{false, -1.0f, -1.0f};
    clock.advance(dt, phase, result);
    return result;
}

rs::Result<std::vector<AnimationAttribute>, std::string> PlaybackClip::attributes(std::uint32_t mask) const {
    std::vector<AnimationAttribute> output;
    for (const ClipAttribute& attribute : attributes_) {
        const std::uint8_t status =
            attribute.begin == -1.0f ? std::uint8_t(4) : clock.attribute_status(attribute.begin, attribute.end);
        if (attribute.begin != -1.0f &&
            ((mask & 0x1cu & std::uint32_t(status)) == 0 || (mask & 3u & std::uint32_t(status)) == 0)) {
            continue;
        }
        auto m = materialize(attribute, status);
        if (m.is_err()) return rs::make_err(m.unwrap_err());
        output.push_back(m.unwrap());
    }
    return output;
}

rs::Result<std::optional<AnimationAttribute>, std::string> PlaybackClip::attribute(AttributeName name,
                                                                                    std::uint32_t mask) const {
    for (const ClipAttribute& attribute : attributes_) {
        if (attribute.name != name) {
            continue;
        }
        const std::uint8_t status = clock.attribute_status(attribute.begin, attribute.end);
        if ((mask & 0x1cu & std::uint32_t(status)) == 0 || (mask & 3u & std::uint32_t(status)) == 0) {
            continue;
        }
        auto m = materialize(attribute, status);
        if (m.is_err()) return rs::make_err(m.unwrap_err());
        return std::optional<AnimationAttribute>(m.unwrap());
    }
    return std::optional<AnimationAttribute>();
}

rs::Result<AnimationAttribute, std::string> PlaybackClip::materialize(const ClipAttribute& attribute,
                                                                      std::uint8_t status) const {
    AttributePayload payload{};
    std::size_t lanes = 0;
    switch (attribute.kind) {
        case 0: lanes = 1; break;
        case 1: lanes = 4; break;
        case 3: lanes = 6; break;
        default: lanes = 0; break;
    }
    if (attribute.payload.size() < lanes) {
        return rs::make_err(std::string("Truncated clip attribute payload"));
    }
    for (std::size_t i = 0; i < lanes; ++i) {
        payload._0[i] = attribute.payload[i];
    }
    if (attribute.kind == 2) {
        // The source deliberately multiplies time*fps*playback_speed;
        // base_speed is not an additional factor in Attribute::Init.
        const float frame = (clock.time * clock.fps) * clock.speed;
        auto sampled = sample_curve(attribute.payload, frame);
        if (sampled.is_err()) return rs::make_err(sampled.unwrap_err());
        payload._0[0] = rs::to_bits(sampled.unwrap());
    }
    AnimationAttribute out;
    out.name = attribute.name;
    out.kind = attribute.kind;
    out.payload = payload;
    out.status = status;
    out.sequence_id = -1;
    out.begin_time = (status & 2) != 0 ? -1.0f : attribute.begin * clock.length;
    out.end_time = (status & 2) != 0 ? -1.0f : attribute.end * clock.length;
    return out;
}

rs::Result<float, std::string> sample_curve(Slice<const std::uint32_t> words, float time) {
    const std::uint32_t* count_word = words.get(1);
    if (!count_word) return rs::make_err(std::string("Missing animation curve count"));
    const std::size_t count = *count_word;
    const std::size_t available = words.size() >= 2 ? words.size() - 2 : 0;
    if (count == 0 || count > available / 2) {
        return rs::make_err(std::string("Truncated/empty animation curve"));
    }
    auto point = [&](std::size_t index) {
        return std::make_pair(rs::from_bits(words[2 + 2 * index]), rs::from_bits(words[3 + 2 * index]));
    };
    std::size_t low = 0;
    std::size_t high = count - 1;
    auto [low_x, low_y] = point(low);
    auto [high_x, high_y] = point(high);
    if (!(time > low_x)) {
        return low_y;
    }
    if (!(time < high_x)) {
        return high_y;
    }
    while (!(low_x > high_x)) {
        const std::size_t middle = (low + high) / 2;
        const auto [x, y] = point(middle);
        if (time > x) {
            low = middle + 1;
            if (low >= count) {
                return rs::make_err(std::string("Malformed animation curve search bounds"));
            }
            std::tie(low_x, low_y) = point(low);
        } else if (time < x) {
            if (middle == 0) return rs::make_err(std::string("Malformed animation curve search bounds"));
            high = middle - 1;
            std::tie(high_x, high_y) = point(high);
        } else {
            return y;
        }
    }
    const float slope = (low_y - high_y) / (low_x - high_x);
    return rs::mul_add(slope, time - high_x, high_y);
}

}  // namespace s3::animation::playback_clip
