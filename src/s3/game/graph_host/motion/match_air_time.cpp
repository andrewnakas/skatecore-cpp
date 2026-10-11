// Port of skate-game/src/graph_host/motion_offboard/match_air_time.rs (module `motion::match_air_time`).
#include "s3/game/graph_host/motion/match_air_time.hpp"

#include <utility>

#include "s3/core/animation/skeleton_input/name.hpp"

namespace s3::game::graph_host::motion::match_air_time {

using s3::animation::playback_parameters::SettableAttribute;
using s3::animation::skeleton_input::name::encode;

void State::begin(float cadence, float duration_, std::array<float, 4> translation_) {
    first_update = true;
    cadence_start = cadence;
    duration = duration_;
    translation = translation_;
}

void State::update(MotionAnimation& animation, float remaining, float duration_, std::array<float, 4> translation_) {
    if (const std::optional<float> fraction = seek_fraction(remaining, duration_)) {
        animation.synchronize_air_time(*fraction);
    }
    translation = bounded_translation(translation_);
    duration = duration_;
    const std::pair<const char*, float> values[] = {
        {"CadenceStartPercent", cadence_start}, {"AnimTime", duration},       {"AnimTransX", translation[0]},
        {"AnimTransY", translation[1]},         {"AnimTransZ", translation[2]},
    };
    for (const auto& [name, value] : values) {
        //82BBA2C0..378: r6=0, r7=-1 for each v80 SetAttribute.
        animation.set_attribute(SettableAttribute{encode(name), value, false, -1});
    }
    first_update = false;
}

std::optional<float> State::seek_fraction(float remaining, float duration_) const {
    //82BBA16C skips the first update;184 is ble (unordered falls through).
    if (first_update || duration_ <= 0.0f) return std::nullopt;
    const float progress = 1.0f - remaining / duration_;
    const float lower = -progress >= 0.0f ? 0.0f : progress;
    return 1.0f - lower >= 0.0f ? lower : 1.0f;
}

std::array<float, 4> bounded_translation(std::array<float, 4> vector) {
    //82BBA218 vmsum3, two rsqrt refinements, zero selection. Independent
    //PC seed, not a claim of bit-exact Xenon reciprocal-square-root emulation.
    const float squared = (vector[0] * vector[0] + vector[1] * vector[1]) + vector[2] * vector[2];
    float inverse = rs::recip(rs::sqrt(squared));
    for (int i = 0; i < 2; ++i) {
        const float correction = rs::mul_add(-squared, inverse * inverse, 1.0f);
        inverse = rs::mul_add(inverse * 0.5f, correction, inverse);
    }
    const float length = squared == 0.0f ? 0.0f : squared * inverse;
    if (length > 0.01f) {
        const float bounded = length - 2.0f >= 0.0f ? 2.0f : length;
        //82BBA2A0 scales all four lanes. Published scalar parameters are XYZ.
        const float scale = bounded / length;
        return {vector[0] * scale, vector[1] * scale, vector[2] * scale, vector[3] * scale};
    }
    return vector;
}

}  // namespace s3::game::graph_host::motion::match_air_time
