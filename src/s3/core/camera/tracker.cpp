// Port of skate-core/src/camera/tracker.rs.
#include "s3/core/camera/tracker.hpp"

namespace s3::camera {

void ScalarTracker::update(float dt, float target_, const ScalarTrackerParameters& parameters) {
    const float position_ = this->position;
    const float delta = target_ - position_;
    this->target = target_;
    if (parameters.overshoot_zeroes_velocity && this->velocity * delta < 0.0f) {
        // Multiplication preserves negative zero; assignment to +0 does not.
        this->velocity *= 0.0f;
    }
    const float velocity_ = this->velocity;
    const float reciprocal_dt = 1.0f / dt;
    this->acceleration = rs::mul_add(reciprocal_dt, delta, -velocity_) * reciprocal_dt;

    const float absolute_delta = rs::abs(delta);
    float fraction;
    if (absolute_delta < parameters.delta_umbra) {
        fraction = 1.0f;
    } else if (absolute_delta < parameters.delta_penumbra) {
        fraction = 1.0f - (absolute_delta - parameters.delta_umbra) /
                              (parameters.delta_penumbra - parameters.delta_umbra);
    } else {
        fraction = 0.0f;
    }

    if (this->acceleration * delta > 0.0f) {
        const float limit =
            tracker::interpolate(parameters.acceleration_clamp_min, parameters.acceleration_clamp_max, fraction);
        this->acceleration_clamp = limit;
        const float absolute_acceleration = rs::abs(this->acceleration);
        if (absolute_acceleration > limit) {
            this->acceleration = (this->acceleration / absolute_acceleration) * limit;
        }
    }

    this->velocity = rs::mul_add(this->acceleration, dt, velocity_);
    const float absolute_velocity = rs::abs(this->velocity);
    if (absolute_velocity > parameters.speed_clamp) {
        this->velocity = (this->velocity / absolute_velocity) * parameters.speed_clamp;
    }

    const float next_position = rs::mul_add(this->velocity, dt, position_);
    this->smoothing = tracker::interpolate(parameters.smoothing_min, parameters.smoothing_max, fraction);
    // Smoothing changes position only. Do not derive velocity back from it.
    this->position = rs::mul_add(next_position - position_, 1.0f - this->smoothing, position_);
}

}  // namespace s3::camera
