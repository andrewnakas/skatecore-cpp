// Port of skate-core/src/player/offboard/contact_toolkit.rs (module-level primitives).
// Split out of contact_toolkit.hpp so the submodule headers can include them.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/reciprocal_sqrt.hpp"
#include "s3/core/rs.hpp"

namespace s3::player::offboard::contact_toolkit {

using Vector = std::array<float, 4>;
using Frame = std::array<Vector, 4>;
inline constexpr Vector UP{0.0f, 1.0f, 0.0f, 0.0f};
inline constexpr Vector ZERO{0.0f, 0.0f, 0.0f, 0.0f};
inline constexpr Frame IDENTITY{{{1.0f, 0.0f, 0.0f, 0.0f}, UP, {0.0f, 0.0f, 1.0f, 0.0f}, ZERO}};

// pub(super)
inline float dot(Vector a, Vector b) { return physics::native_arithmetic::dot3(a, b); }
inline Vector sub(Vector a, Vector b) { return Vector{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
inline Vector scale(Vector a, float s) { return Vector{a[0] * s, a[1] * s, a[2] * s, a[3] * s}; }
inline Vector cross(Vector a, Vector b) {
    return Vector{rs::mul_add(-a[2], b[1], a[1] * b[2]), rs::mul_add(-a[0], b[2], a[2] * b[0]),
                  rs::mul_add(-a[1], b[0], a[0] * b[1]), 0.0f};
}
inline float length(Vector v) {
    const float square = dot(v, v);
    if (square == 0.0f) return 0.0f;
    float inverse = physics::reciprocal_sqrt::estimate(square);
    for (int i = 0; i < 2; ++i) {
        inverse = rs::mul_add(inverse * 0.5f, rs::mul_add(-square, inverse * inverse, 1.0f), inverse);
    }
    return square * inverse;
}

/// Unit face normal shared by the toolkit's scene adapter and query kernels.
inline Vector triangle_normal(const std::array<Vector, 3>& t) {
    const Vector normal = cross(sub(t[1], t[0]), sub(t[2], t[0]));
    const float square = dot(normal, normal);
    float inverse = physics::reciprocal_sqrt::estimate(square);
    for (int i = 0; i < 2; ++i) {
        inverse = rs::mul_add(inverse * 0.5f, rs::mul_add(-square, inverse * inverse, 1.0f), inverse);
    }
    return scale(normal, inverse);
}

struct LineHit {
    Vector position{};
    Vector normal{};
    float fraction = 0.0f;
    std::uint16_t surface = 0;
    Frame mesh_frame{};
    std::uint32_t geometry = 0;
};
S3_DERIVE_EQ(LineHit, position, normal, fraction, surface, mesh_frame, geometry)

}  // namespace s3::player::offboard::contact_toolkit
