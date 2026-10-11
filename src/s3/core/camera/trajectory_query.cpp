// Port of skate-core/src/camera/trajectory_query.rs.
#include "s3/core/camera/trajectory_query.hpp"

namespace s3::camera {

using vector_tracker::dot;
using vector_tracker::length;
using vector_tracker::refined_reciprocal;

F4 TrajectoryQuery::evaluate(float time) const {
    const float square = time * time;
    return rs::from_fn<4>([&](std::size_t i) {
        return rs::mul_add(this->gravity[i] * 0.5f, square, rs::mul_add(this->velocity[i], time, this->position[i]));
    });
}

float TrajectoryQuery::step_size(float error_radius) const {
    const float square = refined_reciprocal(this->gravity[1]) * (-8.0f * error_radius);
    float inverse = physics::reciprocal_sqrt::estimate(square);
    for (int i = 0; i < 2; ++i) {
        inverse = rs::mul_add(inverse * 0.5f, rs::mul_add(-square, inverse * inverse, 1.0f), inverse);
    }
    return square == 0.0f ? 0.0f : square * inverse;
}

float TrajectoryQuery::time_at_contact(F4 contact, float time) const {
    constexpr float FRAME = rs::from_bits(0x3c888889);
    constexpr float EPSILON = rs::from_bits(0x38d1b717);
    if (EPSILON >= rs::abs(refined_reciprocal(2.0f) * this->gravity[1])) {
        const float distance = length(rs::from_fn<4>([&](std::size_t i) { return contact[i] - this->position[i]; }));
        if (!(rs::abs(distance) > EPSILON)) {
            return 0.0f;
        }
        const float speed = length(this->velocity);
        if (!(rs::abs(speed) > EPSILON)) {
            return 0.0f;
        }
        // Preserve8276C794/7B4, including its frame factor. This branch
        // does not use the segment's fractional intersection parameter.
        return (refined_reciprocal(speed) * distance) * FRAME;
    }
    float closest_square = rs::f32::MAX;
    while (this->duration >= time) {
        const F4 point = this->evaluate(time);
        const F4 delta = rs::from_fn<4>([&](std::size_t i) { return contact[i] - point[i]; });
        const float square = dot(delta, delta);
        if (closest_square >= square) {
            closest_square = square;
        } else if (square > closest_square) {
            return time - FRAME;
        }
        time += FRAME;
    }
    return this->duration;
}

}  // namespace s3::camera
