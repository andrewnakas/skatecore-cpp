// Port of skate-core/src/riding/ground_correction_math.rs.
#include "s3/core/riding/ground_correction_math.hpp"

#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::riding::ground_correction_math {

using physics::board_motion_output::dot;
using physics::board_motion_output::inverse_length_squared;
using physics::board_motion_output::length;
using physics::native_arithmetic::reciprocal_estimate;

namespace {

using V4 = std::array<float, 4>;

float reciprocal_refined(float value) {
    const float estimate = reciprocal_estimate(value);
    const float first = rs::mul_add(estimate, rs::mul_add(-estimate, value, 1.0f), estimate);
    return rs::mul_add(first, rs::mul_add(-first, value, 1.0f), first);
}
V4 cross(const V4& a, const V4& b) {
    return {
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
        rs::mul_add(-a[3], b[3], a[3] * b[3]),
    };
}
V4 subtract(const V4& a, const V4& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
Vector3 xyz(const V4& v) { return Vector3{v[0], v[1], v[2]}; }
V4 scaled(const V4& v, float s) {
    return rs::map(v, [s](float x) { return x * s; });
}

}  // namespace

float center_of_mass_height(V4 com_to_deck_world) { return length(xyz(com_to_deck_world)); }

float dot_product(V4 a, V4 b) { return physics::native_arithmetic::dot3(a, b); }

float collision_force_projection(V4 force, V4 velocity) {
    const float squared = dot(xyz(force), xyz(force));
    const float inverse = inverse_length_squared(squared, 2);
    const float len = squared == 0.0f ? 0.0f : squared * inverse;
    const V4 direction = len > rs::from_bits(0x358637bdu) ? scaled(force, inverse) : V4{0.0f, 0.0f, 0.0f, 0.0f};
    return dot(xyz(direction), xyz(velocity));
}

V4 edge_direction(V4 start, V4 end) {
    const V4 delta = subtract(end, start);
    const float inverse = reciprocal_refined(length(xyz(delta)));
    return scaled(delta, inverse);
}

V4 edge_up(V4 direction) {
    const V4 first = cross({0.0f, 1.0f, 0.0f, 0.0f}, direction);
    const V4 perpendicular = cross(first, direction);
    const float magnitude = length(xyz(perpendicular));
    if (magnitude <= 0.0f) {
        return {1.0f, 0.0f, 0.0f, 0.0f};
    }
    const float signed_length = perpendicular[1] < 0.0f ? -magnitude : magnitude;
    const float inverse = reciprocal_refined(signed_length);
    return scaled(perpendicular, inverse);
}

V4 hang_force(V4 edge_start, V4 edge_end, V4 deck) {
    const V4 direction = edge_direction(edge_start, edge_end);
    const V4 up = edge_up(direction);
    V4 side = cross(up, direction);
    if (dot(xyz(subtract(deck, edge_start)), xyz(side)) < 0.0f) {
        side = scaled(side, -1.0f);
    }
    const V4 lift{0.0f, 160.0f, 0.0f, 0.0f};
    V4 out{};
    for (std::size_t i = 0; i < 4; ++i) out[i] = rs::mul_add(side[i], 50.0f, lift[i]);
    return out;
}

V4 wheel_catch_displacement(V4 deck_y, V4 deck_z) {
    const V4 signed_z =
        deck_z[1] > 0.0f ? deck_z : rs::map(deck_z, [](float v) { return rs::from_bits(rs::to_bits(v) ^ 0x80000000u); });
    return scaled(cross(signed_z, deck_y), rs::from_bits(0x3be56042u));
}

V4 pinning_velocity(V4 position, float captured_x, float captured_z, float dt) {
    const V4 target{captured_x, position[1], captured_z, 0.0f};
    const float inverse_dt = reciprocal_refined(dt);
    return scaled(subtract(target, position), inverse_dt);
}

V4 scale_to_magnitude(V4 vector, float squared, float magnitude) {
    const float inverse = inverse_length_squared(squared, 2);
    const float len = squared == 0.0f ? 0.0f : squared * inverse;
    // 82D394D8 divides50 by the computed length with scalar fdivs.
    const float factor = magnitude / len;
    return scaled(vector, factor);
}

void apply_world_force(physics::assembly::BodySnapshot& body, Vector3 deck_part_position, Vector3 force,
                       Vector3 point) {
    const Vector3 arm{point.x - deck_part_position.x, point.y - deck_part_position.y,
                      point.z - deck_part_position.z};
    const V4 torque = cross({arm.x, arm.y, arm.z, 0.0f}, {force.x, force.y, force.z, 0.0f});
    const float inverse = body.inertia.inverse_mass;
    body.rates.force_acceleration.x += force.x * inverse;
    body.rates.force_acceleration.y += force.y * inverse;
    body.rates.force_acceleration.z += force.z * inverse;
    const auto columns = body.rates.world_inverse_inertia.columns;
    const auto angular = [&](std::size_t i) {
        return rs::mul_add(columns[2][i], torque[2], rs::mul_add(columns[1][i], torque[1], columns[0][i] * torque[0]));
    };
    body.rates.torque_acceleration.x += angular(0);
    body.rates.torque_acceleration.y += angular(1);
    body.rates.torque_acceleration.z += angular(2);
    body.rates.cool_down = 0;
}

}  // namespace s3::riding::ground_correction_math
