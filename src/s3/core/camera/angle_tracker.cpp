// Port of skate-core/src/camera/angle_tracker.rs.
#include "s3/core/camera/angle_tracker.hpp"

#include <cstdint>
#include <limits>

namespace s3::camera {

void AngleTracker::update(float dt, float target, const ScalarTrackerParameters& parameters) {
    ScalarTracker& state = this->_0;
    const float position = state.position;
    state.target = target;
    const float delta = normalize_angle(target - position);
    if (parameters.overshoot_zeroes_velocity && state.velocity * delta < 0.0f) {
        state.velocity *= 0.0f;
    }
    const float velocity = state.velocity;
    const float inverse_dt = 1.0f / dt;
    state.acceleration = rs::mul_add(inverse_dt, delta, -velocity) * inverse_dt;
    const float magnitude = rs::abs(delta);
    float fraction;
    if (magnitude < parameters.delta_umbra) {
        fraction = 1.0f;
    } else if (magnitude < parameters.delta_penumbra) {
        fraction = 1.0f - (magnitude - parameters.delta_umbra) / (parameters.delta_penumbra - parameters.delta_umbra);
    } else {
        fraction = 0.0f;
    }
    if (state.acceleration * delta > 0.0f) {
        state.acceleration_clamp =
            tracker::interpolate(parameters.acceleration_clamp_min, parameters.acceleration_clamp_max, fraction);
        const float acceleration_magnitude = rs::abs(state.acceleration);
        if (acceleration_magnitude > state.acceleration_clamp) {
            state.acceleration = (state.acceleration / acceleration_magnitude) * state.acceleration_clamp;
        }
    }
    state.velocity = rs::mul_add(state.acceleration, dt, velocity);
    const float speed = rs::abs(state.velocity);
    if (speed > parameters.speed_clamp) {
        state.velocity = (state.velocity / speed) * parameters.speed_clamp;
    }
    // Native normalizes each intermediate separately. Combining these into
    // a single wrap, or fusing the position addition, changes boundary cases.
    const float next = normalize_angle(normalize_angle(state.velocity * dt) + position);
    state.smoothing = tracker::interpolate(parameters.smoothing_min, parameters.smoothing_max, fraction);
    const float displacement = normalize_angle(next - position);
    const float smoothed = normalize_angle(displacement * (1.0f - state.smoothing));
    state.position = normalize_angle(smoothed + position);
}

float normalize_angle(float angle) {
    constexpr float PI = rs::from_bits(0x40490fdb);
    constexpr float NEG_PI = rs::from_bits(0xc0490fdb);
    constexpr float INV_TAU = rs::from_bits(0x3e22f983);
    constexpr float TAU = rs::from_bits(0x40c90fdb);
    if (angle >= NEG_PI && angle < PI) {
        return angle;
    }
    const float turns = angle * INV_TAU;
    // Ordinary finite truncation follows the native fctiwz operation. Its
    // exceptional/overflow results are unresolved; do not borrow host rules.
    if (!rs::is_finite(turns) || turns < float(std::numeric_limits<std::int32_t>::min()) ||
        double(turns) > double(std::numeric_limits<std::int32_t>::max())) {
        S3_PANIC(
            "Skate 3 exceptional angle integer conversion is unavailable: independent implementation required");
    }
    const std::int32_t count_int = rs::as_i32(turns);
    const float count = float(count_int);
    const float reduced = float(-std::fma(double(count), double(TAU), -double(angle)));
    if (reduced >= PI) {
        return reduced - TAU;
    } else if (reduced < NEG_PI) {
        return reduced + TAU;
    }
    return reduced;
}

}  // namespace s3::camera
