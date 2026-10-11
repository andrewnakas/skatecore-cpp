// Port of skate-core/src/air/trajectory/math.rs.
// Independent PC arithmetic retaining the recovered refinement sequences.
#pragma once
#include <array>

#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::air::trajectory::math {

using Vector = std::array<float, 4>;
using Transform = std::array<Vector, 4>;
inline constexpr Vector ZERO{0.0f, 0.0f, 0.0f, 0.0f};
inline constexpr Vector UP{0.0f, 1.0f, 0.0f, 0.0f};
inline const float STEP = rs::from_bits(0x3c888889u);
inline constexpr Transform IDENTITY{{{1.0f, 0.0f, 0.0f, 0.0f}, UP, {0.0f, 0.0f, 1.0f, 0.0f}, ZERO}};

inline float dot(Vector a, Vector b) { return physics::native_arithmetic::dot3(a, b); }
inline Vector add(Vector a, Vector b) { return Vector{a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]}; }
inline Vector sub(Vector a, Vector b) { return Vector{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
inline Vector scale(Vector a, float s) { return Vector{a[0] * s, a[1] * s, a[2] * s, a[3] * s}; }
inline Vector madd(Vector a, float s, Vector b) {
    return Vector{rs::mul_add(a[0], s, b[0]), rs::mul_add(a[1], s, b[1]), rs::mul_add(a[2], s, b[2]),
                  rs::mul_add(a[3], s, b[3])};
}
inline Vector cross(Vector a, Vector b) {
    return Vector{
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
        0.0f,
    };
}
inline float inverse_length(float square) {
    float v = physics::native_arithmetic::reciprocal_square_root_estimate(square);
    for (int i = 0; i < 2; ++i) {
        v = rs::mul_add(v * 0.5f, rs::mul_add(-square, v * v, 1.0f), v);
    }
    return v;
}
inline float reciprocal(float value) {
    float inverse = physics::native_arithmetic::reciprocal_estimate(value);
    for (int i = 0; i < 2; ++i) {
        inverse = rs::mul_add(inverse, rs::mul_add(-inverse, value, 1.0f), inverse);
    }
    return inverse;
}
///8296EBB0: squared-length gate, ONE refinement, clamped dot, native acos.
inline float angle_between(Vector a, Vector b) {
    const float aa = dot(a, a);
    const float bb = dot(b, b);
    if (!(aa > rs::from_bits(0x38d1b717u) && bb > rs::from_bits(0x38d1b717u))) {
        return 0.0f;
    }
    auto unit = [](Vector v, float square) {
        const float r = physics::native_arithmetic::reciprocal_square_root_estimate(square);
        return scale(v, rs::mul_add(r * 0.5f, rs::mul_add(-square, r * r, 1.0f), r));
    };
    return trigonometry::acos(rs::min(rs::max(dot(unit(a, aa), unit(b, bb)), -1.0f), 1.0f));
}
inline float length(Vector a) {
    const float square = dot(a, a);
    if (square == 0.0f) {
        return 0.0f;
    }
    return square * inverse_length(square);
}
inline Vector normalize(Vector a) {
    const float square = dot(a, a);
    const float inverse = inverse_length(square);
    const float length = square == 0.0f ? 0.0f : square * inverse;
    //830BD350's initializer82F826F8 broadcasts82181A88 (1e-6).
    if (length > rs::from_bits(0x358637bdu)) {
        return scale(a, inverse);
    }
    return ZERO;
}
inline Vector transform(const Transform& m, Vector a) {
    return madd(m[2], a[2], madd(m[1], a[1], madd(m[0], a[0], m[3])));
}

}  // namespace s3::air::trajectory::math
