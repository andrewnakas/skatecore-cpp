// Port of skate-core/src/animation/intent_filter.rs.
#include "s3/core/animation/intent_filter.hpp"

#include "s3/core/animation/compat/input_graph_intents.hpp"

namespace s3::animation::intent_filter {

using input::graph_intents::apply_filter;

float State::begin(const Settings& settings) {
    elapsed = 0.0f;
    previous_delta = 0.0f;
    value = settings.starting_value;
    return value;
}

float State::update(const Settings& settings, std::optional<float> input, float dt, std::pair<bool, bool> stance) {
    elapsed += dt;
    float target = settings.scale * (input ? *input : settings.default_value);
    const std::array<bool, 4> enabled = {true, true, stance.first, stance.second};
    for (std::size_t i = 0; i < 4; ++i) {
        const std::uint32_t kind = settings.filters[i];
        if (enabled[i] && kind <= 7) {
            target = apply_filter(target, kind);
        }
    }
    float blend = target < value ? settings.blend_falling : settings.blend_rising;
    if (input) {
        if (settings.ramp_time) {
            blend = ramp(elapsed, *settings.ramp_time) * blend;
        }
    } else if (settings.blend_out) {
        blend = *settings.blend_out;
    }
    // 82BB194C..1968: separate target product, then fmadds, then subtract.
    const float weighted_target = blend * target;
    const float candidate = rs::mul_add(1.0f - blend, value, weighted_target);
    float delta = candidate - value;
    if (settings.clamp_acceleration) {
        const float limit = *settings.clamp_acceleration;
        delta = limit_delta(delta, previous_delta - limit, previous_delta + limit);
    }
    if (settings.clamp_velocity) {
        const float limit = *settings.clamp_velocity;
        delta = limit_delta(delta, -limit, limit);
    }
    previous_delta = delta;
    value = delta + value;
    return value;
}

float limit_delta(float value, float lower, float upper) {
    // Preserve the original subtract/select ordering, including unordered input.
    const float selected = lower - value >= 0.0f ? lower : value;
    return upper - selected >= 0.0f ? selected : upper;
}

float ramp(float elapsed, float time) {
    // 82BA8720 uses signed integer comparisons of float bits in lower_bound.
    // Constructor 82BB1570..15BC supplies precisely (0,0),(rampTime,1).
    const std::array<float, 2> keys = {0.0f, time};
    std::size_t begin = 0;
    std::size_t count = 2;
    while (count != 0) {
        const std::size_t step = count / 2;
        const std::size_t middle = begin + step;
        if (static_cast<std::int32_t>(rs::to_bits(keys[middle])) < static_cast<std::int32_t>(rs::to_bits(elapsed))) {
            begin = middle + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    switch (begin) {
        case 0: return 0.0f;
        case 2: return 1.0f;
        default: return rs::mul_add((1.0f - 0.0f) / (time - 0.0f), elapsed - 0.0f, 0.0f);
    }
}

}  // namespace s3::animation::intent_filter
