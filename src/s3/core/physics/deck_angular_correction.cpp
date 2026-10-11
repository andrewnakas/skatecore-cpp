// Port of skate-core/src/physics/deck_angular_correction.rs.
#include "s3/core/physics/deck_angular_correction.hpp"

#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::physics::deck_angular_correction {

using board_motion_output::dot;
using board_motion_output::inverse_length_squared;
using native_arithmetic::reciprocal_estimate;

namespace {
constexpr float STEP = rs::from_bits(0x3c888889u);
constexpr float NORMAL_MINIMUM = rs::from_bits(0x358637bdu);  // 82F826F8 ->830BD350.

float reciprocal_refined(float value) {
    const float estimate = reciprocal_estimate(value);
    const float first = rs::mul_add(estimate, rs::mul_add(-estimate, value, 1.0f), estimate);
    return rs::mul_add(first, rs::mul_add(-first, value, 1.0f), first);
}
Vector3 cross(Vector3 a, Vector3 b) {
    return Vector3{
        rs::mul_add(-a.z, b.y, a.y * b.z),
        rs::mul_add(-a.x, b.z, a.z * b.x),
        rs::mul_add(-a.y, b.x, a.x * b.y),
    };
}
Vector3 multiply(const Basis3& matrix, Vector3 v) {
    auto lane = [&](std::size_t i) {
        return rs::mul_add(matrix.columns[2][i], v.z,
                           rs::mul_add(matrix.columns[1][i], v.y, matrix.columns[0][i] * v.x));
    };
    return Vector3{lane(0), lane(1), lane(2)};
}
Vector3 scale(Vector3 v, float x) { return Vector3{v.x * x, v.y * x, v.z * x}; }
Vector3 subtract(Vector3 a, Vector3 b) { return Vector3{a.x - b.x, a.y - b.y, a.z - b.z}; }

Basis3 invert_symmetric(const Basis3& tensor) {
    auto col = [&](std::size_t i) {
        return Vector3{tensor.columns[i][0], tensor.columns[i][1], tensor.columns[i][2]};
    };
    const Vector3 a = col(0), b = col(1), c = col(2);
    const Vector3 first = cross(b, c);
    const Vector3 second = cross(c, a);
    const Vector3 third = cross(a, b);
    const float inverse_det = reciprocal_refined(dot(a, first));
    return Basis3{{{
        {first.x * inverse_det, second.x * inverse_det, third.x * inverse_det},
        {first.y * inverse_det, second.y * inverse_det, third.y * inverse_det},
        {first.z * inverse_det, second.z * inverse_det, third.z * inverse_det},
    }}};
}

float ground_torque_scalar() {
    // Positive fixed input10 has exponent3 and mantissa1.25. Thus the native
    // vlog estimate followed by floor selects3; its sign/special-value masks
    // cannot select another branch for these immutable instruction inputs.
    const float x = 0.25f;
    float square = x * x;
    float cube = x * square;
    float fourth = square * square;
    const auto log = rs::from_bits(std::array<std::uint32_t, 8>{
        0x3fb8aa0eu, 0xbf389e52u, 0x3ef5162du, 0xbeb1d204u, 0x3e77adbdu, 0xbe0cd4fbu, 0x3d5541c6u, 0xbc188b0bu});
    float low = rs::mul_add(cube, log[3], rs::mul_add(square, log[2], rs::mul_add(x, log[1], log[0])));
    float high = rs::mul_add(cube, log[7], rs::mul_add(square, log[6], rs::mul_add(x, log[5], log[4])));
    const float exponent = rs::mul_add(-x, rs::mul_add(fourth, high, low), -3.0f);
    const float integral = rs::floor(exponent);
    const float fraction = exponent - integral;
    square = fraction * fraction;
    cube = fraction * square;
    fourth = square * square;
    const auto exp = rs::from_bits(std::array<std::uint32_t, 8>{
        0x3f800000u, 0xbf317218u, 0x3e75fdedu, 0xbd6357cau, 0x3c1d8c54u, 0xbaae1854u, 0x391aa7d7u, 0xb7364261u});
    low = rs::mul_add(cube, exp[3], rs::mul_add(square, exp[2], rs::mul_add(fraction, exp[1], exp[0])));
    high = rs::mul_add(cube, exp[7], rs::mul_add(square, exp[6], rs::mul_add(fraction, exp[5], exp[4])));
    const float polynomial = rs::mul_add(fourth, high, low);
    // vexptefp sees an integral exponent here, so its power of two is exact.
    const float power_of_two = rs::from_bits(static_cast<std::uint32_t>(127 + rs::as_i32(integral)) << 23);
    return (power_of_two * reciprocal_refined(polynomial)) * rs::from_bits(0xc0c1999au);
}
}  // namespace

void apply_axis_displacement(RetailBodyRates& body, Vector3 requested) {
    const float squared = dot(requested, requested);
    const float inverse = inverse_length_squared(squared, 2);
    const float length = squared == 0.0f ? 0.0f : squared * inverse;
    const Vector3 direction = length > NORMAL_MINIMUM ? scale(requested, inverse) : Vector3::ZERO;
    const Vector3 existing = scale(direction, dot(direction, scale(body.angular_velocity, STEP)));
    apply_angular_displacement(body, subtract(requested, existing));
}

void apply_limited_displacement(RetailBodyRates& body, Vector3 requested) {
    const float squared = dot(requested, requested);
    const float inverse = inverse_length_squared(squared, 2);
    const float length = squared == 0.0f ? 0.0f : squared * inverse;
    const Vector3 direction = length > NORMAL_MINIMUM ? scale(requested, inverse) : Vector3::ZERO;
    const Vector3 existing = scale(direction, dot(direction, scale(body.angular_velocity, STEP)));
    Vector3 remainder = subtract(requested, existing);
    remainder = dot(remainder, requested) < 0.0f ? Vector3::ZERO : remainder;
    const Vector3 selected = dot(existing, requested) < 0.0f ? requested : remainder;
    apply_angular_displacement(body, selected);
}

void apply_angular_displacement(RetailBodyRates& body, Vector3 displacement) {
    const Basis3 tensor = body.world_inverse_inertia;
    const Basis3 inverse = invert_symmetric(tensor);
    const float inverse_step = reciprocal_refined(STEP);
    const Vector3 target_rate = scale(displacement, inverse_step);
    const Vector3 momentum = multiply(inverse, target_rate);
    const Vector3 torque = scale(momentum, reciprocal_refined(STEP));
    const Vector3 acceleration = multiply(tensor, torque);
    body.torque_acceleration = Vector3{
        body.torque_acceleration.x + acceleration.x,
        body.torque_acceleration.y + acceleration.y,
        body.torque_acceleration.z + acceleration.z,
    };
    // Native accumulator +172 shares its vector with the cooldown word.
    body.cool_down = 0;
}

void apply_ground_body_torque(RetailBodyRates& body) {
    const Vector3 local{0.0f, ground_torque_scalar(), 0.0f};
    const Vector3 world = multiply(body.basis, local);
    const Vector3 acceleration = multiply(body.world_inverse_inertia, world);
    body.torque_acceleration = Vector3{
        acceleration.x + body.torque_acceleration.x,
        acceleration.y + body.torque_acceleration.y,
        acceleration.z + body.torque_acceleration.z,
    };
    body.cool_down = 0;
}

}  // namespace s3::physics::deck_angular_correction
