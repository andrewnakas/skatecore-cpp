// Port of skate-core/src/air/ground_jump/math.rs.
// Original arithmetic ordering and two estimate-refinement steps.
#pragma once
#include <array>

#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/rs.hpp"

namespace s3::air::ground_jump::math {

using Vector = std::array<float, 4>;
inline constexpr Vector UP{0.0f, 1.0f, 0.0f, 0.0f};

inline float dot(Vector a, Vector b) { return physics::native_arithmetic::dot3(a, b); }
inline Vector add(Vector a, Vector b) { return Vector{a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]}; }
inline Vector sub(Vector a, Vector b) { return Vector{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
inline Vector scale(Vector a, float b) { return Vector{a[0] * b, a[1] * b, a[2] * b, a[3] * b}; }
inline Vector madd(Vector a, float b, Vector c) {
    return Vector{rs::mul_add(a[0], b, c[0]), rs::mul_add(a[1], b, c[1]), rs::mul_add(a[2], b, c[2]),
                  rs::mul_add(a[3], b, c[3])};
}
inline Vector planar(Vector value, Vector normal) { return sub(value, scale(normal, dot(value, normal))); }
inline float maximum(float a, float b) { return a - b >= -0.0f ? a : b; }
inline float clamp(float v, float low, float high) {
    const float lower = low - v >= -0.0f ? low : v;
    return high - lower >= -0.0f ? lower : high;
}
inline float vector_clamp(float v, float low, float high) {
    return physics::native_arithmetic::vector_min(high, physics::native_arithmetic::vector_max(low, v));
}
inline float reciprocal(float v) {
    float r = physics::native_arithmetic::reciprocal_estimate(v);
    for (int i = 0; i < 2; ++i) {
        r = rs::mul_add(r, rs::mul_add(-r, v, 1.0f), r);
    }
    return r;
}
inline float square_root(float square) {
    const float value = square * physics::board_motion_output::inverse_length_squared(square, 2);
    return square == 0.0f ? 0.0f : value;
}
inline float length(Vector value) { return square_root(dot(value, value)); }
inline Vector normalize(Vector value) {
    const float square = dot(value, value);
    const float inverse = physics::board_motion_output::inverse_length_squared(square, 2);
    const float length = square == 0.0f ? 0.0f : square * inverse;
    // 82F826F8 initializes830BD350 from82181A88 (1e-6), not its IDA BSS zero.
    if (length > rs::from_bits(0x358637bdu)) {
        return scale(value, inverse);
    }
    return Vector{};
}
inline Vector cross(Vector a, Vector b) {
    return Vector{
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
        0.0f,
    };
}

}  // namespace s3::air::ground_jump::math
