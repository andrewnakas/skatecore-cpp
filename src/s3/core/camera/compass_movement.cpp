// Port of skate-core/src/camera/compass_movement.rs.
//! Movement compass82DF3AC0 and its two-dimensional dead-zone82DF9AD0.
#include "s3/core/camera/compass.hpp"
#include "s3/core/camera/direction.hpp"
#include "s3/core/camera/manager_state.hpp"
#include "s3/core/camera/orientation_math.hpp"
#include "s3/core/camera/vector_tracker.hpp"

namespace s3::camera {

using compass::blend;
using compass::horizontal;
using compass::sub;
using manager_state::clamp;
using vector_tracker::length;
using vector_tracker::refined_reciprocal;

void Compass::update_movement(float dt, const CompassInputs& input, const CompassSettings& s) {
    if (!input.no_trajectory) {
        return;
    }
    const bool reset = dt == 0.0f || this->previous_reset;
    if (reset) {
        this->velocity = F4{};
        this->damped_velocity = F4{};
    } else {
        const float inverse = refined_reciprocal(dt);
        this->velocity = rs::map(sub(input.transform[3], this->previous_position), [&](float v) { return v * inverse; });
        const float speed = length(horizontal(this->velocity));
        const float fraction = clamp(
            (speed - s.maximum_deadzone_speed) / (s.minimum_deadzone_speed - s.maximum_deadzone_speed), 0.0f, 1.0f);
        this->deadzone_size = rs::mul_add(rs::mul_add(fraction, s.maximum_deadzone_size, -this->deadzone_size),
                                          1.0f - s.deadzone_smoothing, this->deadzone_size);
        this->update_deadzone(dt, {input.transform[3][0], input.transform[3][2]});
        this->damped_velocity = {this->deadzone_velocity[0], 0.0f, this->deadzone_velocity[1],
                                 this->deadzone_velocity[0]};
        this->previous_position = input.transform[3];
    }
    const float speed = length(horizontal(this->velocity));
    if (reset) {
        this->movement_heading = -direction_to_angles(input.skeleton_direction)[1];
    } else if (input.grinding) {
        this->movement_heading = -direction_to_angles(input.grind_direction)[1];
    } else {
        float target = this->movement_heading;
        float weight = 1.0f;
        this->stopped_time = (input.wiping_out || speed > 0.5f) ? 0.0f : this->stopped_time + dt;
        if (speed > 0.5f) {
            weight = clamp(s.heading_response, 0.0f, 1.0f);
            target = -direction_to_angles(orientation_math::normalize(this->damped_velocity))[1];
        } else if (!input.state_103 && this->stopped_time >= s.time_before_lineup) {
            weight = s.lineup_speed;
            target = -direction_to_angles(input.transform[2])[1];
        }
        this->movement_heading = blend(this->movement_heading, target, weight);
    }
}

void Compass::update_deadzone(float dt, std::array<float, 2> target) {
    const std::array<float, 2> old = this->deadzone_position;
    const std::array<float, 2> difference = {target[0] - old[0], target[1] - old[1]};
    // The source explicitly sums two squared lanes, not vmsum3fp.
    const float square = difference[0] * difference[0] + difference[1] * difference[1];
    float inverse = physics::reciprocal_sqrt::estimate(square);
    for (int k = 0; k < 2; ++k) {
        inverse = rs::mul_add(inverse * 0.5f, rs::mul_add(-square, inverse * inverse, 1.0f), inverse);
    }
    const float distance = square == 0.0f ? 0.0f : square * inverse;
    if (distance > this->deadzone_size) {
        this->deadzone_position = rs::from_fn<2>([&](std::size_t i) {
            return rs::mul_add(difference[i] * inverse, distance - this->deadzone_size, old[i]);
        });
    }
    if (dt == 0.0f) {
        this->deadzone_velocity = {0.0f, 0.0f};
    } else {
        this->deadzone_velocity =
            rs::from_fn<2>([&](std::size_t i) { return (this->deadzone_position[i] - old[i]) * (1.0f / dt); });
    }
}

}  // namespace s3::camera
