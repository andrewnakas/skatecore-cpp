// Port of skate-core/src/camera/vector_tracker.rs.
#include "s3/core/camera/vector_tracker.hpp"

namespace s3::camera {

using vector_tracker::dot;
using vector_tracker::length;
using vector_tracker::refined_reciprocal;

void VectorTracker::update(float dt, F4 target_, const ScalarTrackerParameters& p) {
    this->target = target_;
    const F4 old = this->position;
    const F4 delta = rs::from_fn<4>([&](std::size_t i) { return target_[i] - old[i]; });
    if (p.overshoot_zeroes_velocity && dot(delta, this->velocity) < 0.0f) {
        this->velocity = rs::map(this->velocity, [](float v) { return v * 0.0f; });
    }
    const float inverse_dt = refined_reciprocal(dt);
    this->acceleration = rs::from_fn<4>([&](std::size_t i) {
        // Separate VMX multiply/subtract/multiply, unlike scalar FMSUBS.
        return (inverse_dt * delta[i] - this->velocity[i]) * inverse_dt;
    });
    const float magnitude = length(delta);
    float fraction;
    if (magnitude < p.delta_umbra) {
        fraction = 1.0f;
    } else if (magnitude < p.delta_penumbra) {
        fraction = 1.0f - (magnitude - p.delta_umbra) / (p.delta_penumbra - p.delta_umbra);
    } else {
        fraction = 0.0f;
    }
    if (dot(this->acceleration, delta) > 0.0f) {
        this->acceleration_clamp = tracker::interpolate(p.acceleration_clamp_min, p.acceleration_clamp_max, fraction);
        this->acceleration = clamp_magnitude(this->acceleration, this->acceleration_clamp);
    }
    this->velocity =
        rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(this->acceleration[i], dt, this->velocity[i]); });
    this->velocity = clamp_magnitude(this->velocity, p.speed_clamp);
    const F4 next = rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(this->velocity[i], dt, old[i]); });
    this->smoothing = tracker::interpolate(p.smoothing_min, p.smoothing_max, fraction);
    this->position = rs::from_fn<4>(
        [&](std::size_t i) { return rs::mul_add(next[i] - old[i], 1.0f - this->smoothing, old[i]); });
}

}  // namespace s3::camera
