// Port of skate-core/src/camera/manager_incline.rs.
#include "s3/core/camera/manager_incline.hpp"

#include "s3/core/camera/angle_tracker.hpp"
#include "s3/core/camera/direction.hpp"
#include "s3/core/camera/manager_state.hpp"
#include "s3/core/camera/manager_subject.hpp"
#include "s3/core/camera/orientation_math.hpp"
#include "s3/core/camera/vector_tracker.hpp"
#include "s3/core/input/angle.hpp"
#include "s3/core/physics/board_ground.hpp"

namespace s3::camera {

using manager_state::HALF_PI;
using manager_state::UP;
using vector_tracker::dot;
using vector_tracker::length;

void ManagerState::update_incline(const ManagerSubject& subject) {
    this->ground_normal = subject.ground_normal;
    const F4 normal = subject.rig.last_valid_ground_up;
    if (normal[1] > rs::from_bits(0x3f7fff58) && subject.rig.grinding == 0) {
        this->velocity_incline = 0.0f;
        this->absolute_velocity_incline = 0.0f;
        this->frontside_angle = 0.0f;
        this->ground_heading = 0.0f;
        this->direction_incline = 0.0f;
        this->flags &= std::uint8_t(~0x20u);
        return;
    }
    const F4 direction = length(this->anchor_velocity) > rs::from_bits(0x358637bd)
                             ? orientation_math::normalize(this->anchor_velocity)
                             : subject.rig.transform[2];
    this->velocity_incline = HALF_PI - manager_incline::angle_between(UP, direction);
    this->absolute_velocity_incline = this->velocity_incline < 0.0f ? -this->velocity_incline : this->velocity_incline;
    this->ground_heading = direction_to_angles(normal)[1];
    // Projection and subtraction are separate VMX operations, followed
    // by the two-refinement normalization used inline at82E00554.
    const float up_dot = dot(UP, normal);
    const F4 projected = rs::map(normal, [&](float v) { return v * up_dot; });
    const F4 tangent =
        orientation_math::normalize(rs::from_fn<4>([&](std::size_t i) { return UP[i] - projected[i]; }));
    const F4 cross = {
        rs::mul_add(-normal[2], tangent[1], normal[1] * tangent[2]),
        rs::mul_add(-normal[0], tangent[2], normal[2] * tangent[0]),
        rs::mul_add(-normal[1], tangent[0], normal[0] * tangent[1]),
        0.0f,
    };
    const float denominator = dot(direction, tangent);
    const float numerator = dot(direction, cross);
    this->frontside_angle = manager_incline::atan_ratio(numerator, denominator);
    this->flags = std::uint8_t((this->flags & ~0x20u) | (this->frontside_angle > 0.0f ? 0x20u : 0u));
    if (this->mirrored()) {
        this->frontside_angle *= -1.0f;
    }
    // vrlimi mask1 replaces W with X; this condition tests XYZ only.
    const float tiny = rs::from_bits(0x34000000);
    if (rs::abs(this->anchor_velocity[0]) > tiny || rs::abs(this->anchor_velocity[1]) > tiny ||
        rs::abs(this->anchor_velocity[2]) > tiny) {
        this->direction_incline =
            normalize_angle(HALF_PI - manager_incline::angle_between(subject.direction_424, UP));
    }
}

namespace manager_incline {

float angle_between(F4 a, F4 b) {
    auto to_vector = [](const F4& v) { return math::Vector3::new_(v[0], v[1], v[2]); };
    return physics::board_ground::angle_between(to_vector(a), to_vector(b));
}

float atan_ratio(float numerator, float denominator) {
    const float initial = physics::native_arithmetic::reciprocal_estimate(denominator);
    const float inverse = rs::mul_add(initial, rs::mul_add(-initial, denominator, 1.0f), initial);
    float angle = input::angle::atan(rs::mul_add(numerator, inverse, 0.0f));
    const std::uint32_t sign = rs::to_bits(numerator) & 0x80000000u;
    if (0.0f > denominator) {
        angle = rs::from_bits(0x40490fdbu | sign) + angle;
    }
    if (denominator == 0.0f) {
        angle = rs::from_bits(0x3fc90fdbu | sign);
    }
    return angle;
}

}  // namespace manager_incline
}  // namespace s3::camera
