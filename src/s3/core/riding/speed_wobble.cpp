// Port of skate-core/src/riding/speed_wobble.rs.
#include "s3/core/riding/speed_wobble.hpp"

#include "s3/core/trigonometry.hpp"

namespace s3::riding::speed_wobble {

namespace {

float unit(float value) {
    const float lower = -value >= 0.0f ? 0.0f : value;
    return 1.0f - lower >= 0.0f ? lower : 1.0f;
}

}  // namespace

float calculate(SpeedWobbleState& state, const SpeedWobbleSettings& settings, SpeedWobbleInput input) {
    const float height =
        (input.center_of_mass_height - settings.height_min) / (settings.height_max - settings.height_min);
    const float crouch = unit(1.0f - height);
    const float threshold =
        rs::mul_add(crouch, settings.crouch_threshold, settings.tightness_threshold * input.truck_tightness) +
        input.activation_threshold;
    if (!state.active() && input.speed > threshold) {
        state.reset();
        state._0[7] |= 0x01000000u;
    }
    if (!state.active()) {
        return input.tilt;
    }

    const float time = rs::from_bits(state._0[0]);
    const float time_fraction = unit(time / settings.time_range);
    const float speed_fraction = unit((input.speed - threshold) / settings.speed_range);
    const float frequency =
        (settings.frequency_time.evaluate(time_fraction) * settings.frequency_speed.evaluate(speed_fraction)) *
        settings.frequency_scale;
    const float amplitude =
        settings.amplitude_speed.evaluate(speed_fraction) * settings.amplitude_time.evaluate(time_fraction);
    const float phase = rs::mul_add(frequency, rs::from_bits(0x3DD67751u), rs::from_bits(state._0[4]));
    const float scale = (settings.amplitude_scale * amplitude) * input.amplitude_multiplier;
    const float displacement = trigonometry::sin(phase) * scale;
    state._0[0] = rs::to_bits(time + rs::from_bits(0x3C888889u));
    state._0[4] = rs::to_bits(phase);
    state._0[5] = rs::to_bits(amplitude);
    state._0[6] = rs::to_bits(displacement);
    const float result = (displacement + input.tilt) * 0.5f;
    // This frame still returns the computed tilt when speed drops below threshold.
    if (input.speed < threshold) {
        state.reset();
    }
    return result;
}

}  // namespace s3::riding::speed_wobble
