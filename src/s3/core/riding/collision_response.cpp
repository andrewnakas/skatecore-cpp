// Port of skate-core/src/riding/collision_response.rs.
#include "s3/core/riding/collision_response.hpp"

#include "s3/core/physics/board_ground.hpp"
#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::riding::collision_response {

using physics::board_ground::angle_between;
using physics::board_motion_output::dot;
using physics::board_motion_output::inverse_length_squared;
using physics::board_motion_output::length;
using physics::native_arithmetic::reciprocal_estimate;
using V4 = std::array<float, 4>;

namespace {
/// 8258DB98 scalar truncation, fused wrap subtraction, then single range repair.
float wrap_angle(float angle) {
    const float pi = rs::from_bits(0x40490fdbu);
    const float tau = rs::from_bits(0x40c90fdbu);
    if (angle < -pi || !(angle < pi)) {
        const std::int32_t turns = rs::as_i32(angle * rs::from_bits(0x3e22f983u));
        angle = rs::mul_add(-static_cast<float>(turns), tau, angle);
        if (!(angle < pi)) {
            angle -= tau;
        } else if (angle < -pi) {
            angle += tau;
        }
    }
    return angle;
}
float reciprocal_refined(float value) {
    const float estimate = reciprocal_estimate(value);
    const float first = rs::mul_add(estimate, rs::mul_add(-estimate, value, 1.0f), estimate);
    return rs::mul_add(first, rs::mul_add(-first, value, 1.0f), first);
}
Vector3 scale(Vector3 v, float k) { return Vector3{v.x * k, v.y * k, v.z * k}; }
Vector3 xyz(const V4& v) { return Vector3{v[0], v[1], v[2]}; }
}  // namespace

std::optional<CollisionResponse> collision_response(const CollisionResponseSettings& settings,
                                                    CollisionResponsePhysical input) {
    if ((input.flags_2472 & 0x20000u) == 0) {
        return std::nullopt;
    }
    const float projection = dot(xyz(input.collision_displacement), xyz(input.ground_normal));
    V4 displaced{};
    for (std::size_t i = 0; i < 4; ++i) {
        displaced[i] = input.collision_displacement[i] - input.ground_normal[i] * projection;
    }
    const float distance = length(xyz(displaced));
    if (distance < rs::from_bits(0x37800000u)) {
        return std::nullopt;
    }
    const float inverse_distance = 1.0f / distance;  // scalar fdivs, not vector estimate.
    V4 direction{};
    for (std::size_t i = 0; i < 4; ++i) direction[i] = displaced[i] * inverse_distance;
    const float speed_toward = dot(xyz(input.velocity), xyz(direction));
    V4 tangent{};
    for (std::size_t i = 0; i < 4; ++i) tangent[i] = input.velocity[i] - direction[i] * speed_toward;
    V4 target_velocity{};
    for (std::size_t i = 0; i < 4; ++i) {
        target_velocity[i] = rs::mul_add(direction[i], settings.target_displacement_velocity, tangent[i]);
    }
    V4 delta{};
    for (std::size_t i = 0; i < 4; ++i) {
        delta[i] = (target_velocity[i] - input.velocity[i]) * settings.force_scalar;
    }
    if (speed_toward > settings.target_displacement_velocity) {
        delta = V4{0.0f, 0.0f, 0.0f, 0.0f};
    }
    const float magnitude = length(xyz(delta));
    const float inverse_dt = reciprocal_refined(input.time_step);
    V4 force{};
    for (std::size_t i = 0; i < 4; ++i) force[i] = (delta[i] * input.mass) * inverse_dt;
    const bool applied = magnitude >= 0.01f;
    if (applied) {
        const float clamp =
            magnitude <= settings.maximum_velocity_delta ? 1.0f : settings.maximum_velocity_delta / magnitude;
        for (float& v : force) v = v * clamp;
    } else {
        force = V4{0.0f, 0.0f, 0.0f, 0.0f};
    }
    const float angle = wrap_angle(signed_angle(xyz(input.forward), xyz(direction), xyz(input.up)));
    const float torque = settings.torque_vs_angle.evaluate(rs::abs(angle) * rs::from_bits(0x3ea2f983u));
    const float signed_ = angle >= 0.0f ? torque : -torque;
    V4 angular{};
    for (std::size_t i = 0; i < 4; ++i) angular[i] = input.up[i] * signed_;
    return CollisionResponse{
        applied, force, V4{0.0f, settings.force_y_offset, 0.0f, 0.0f}, angular, target_velocity,
    };
}

float signed_angle(Vector3 a_in, Vector3 b_in, Vector3 up) {
    const float a_squared = dot(a_in, a_in);
    const float b_squared = dot(b_in, b_in);
    if (!(a_squared > 0.0001f && b_squared > 0.0001f)) {
        return 0.0f;
    }
    const float angle = angle_between(a_in, b_in);
    const Vector3 a = scale(a_in, inverse_length_squared(a_squared, 1));
    const Vector3 b = scale(b_in, inverse_length_squared(b_squared, 1));
    const Vector3 cross{
        rs::mul_add(-a.z, b.y, a.y * b.z),
        rs::mul_add(-a.x, b.z, a.z * b.x),
        rs::mul_add(-a.y, b.x, a.x * b.y),
    };
    if (dot(cross, up) < 0.0f) {
        return rs::from_bits(0x40c90fdbu) - angle;
    }
    return angle;
}

}  // namespace s3::riding::collision_response
