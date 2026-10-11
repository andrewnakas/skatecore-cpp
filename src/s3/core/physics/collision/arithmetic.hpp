// Port of skate-core/src/physics/collision/arithmetic.rs.
#pragma once
#include <cstdint>

#include "s3/core/math.hpp"
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/reciprocal_sqrt.hpp"

namespace s3::physics::collision::arithmetic {

using V = math::Vector3;

inline float dot(V a, V b) {
    return native_arithmetic::dot3({a.x, a.y, a.z, 0.0f}, {b.x, b.y, b.z, 0.0f});
}
inline V cross(V a, V b) {
    return V{
        rs::mul_add(-a.z, b.y, a.y * b.z),
        rs::mul_add(-a.x, b.z, a.z * b.x),
        rs::mul_add(-a.y, b.x, a.x * b.y),
    };
}
inline V scale(V a, float b) { return V{a.x * b, a.y * b, a.z * b}; }
inline V neg(V a) { return V{-a.x, -a.y, -a.z}; }
inline V sub(V a, V b) { return V{a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V madd(V a, float b, V c) {
    return V{rs::mul_add(a.x, b, c.x), rs::mul_add(a.y, b, c.y), rs::mul_add(a.z, b, c.z)};
}
inline float inverse_length_squared(float value, std::uint32_t refinements) {
    float r = reciprocal_sqrt::estimate(value);
    for (std::uint32_t i = 0; i < refinements; ++i) {
        r = rs::mul_add(r * 0.5f, rs::mul_add(-value, r * r, 1.0f), r);
    }
    return r;
}
inline V normalize(V a, std::uint32_t refinements) {
    return scale(a, inverse_length_squared(dot(a, a), refinements));
}

}  // namespace s3::physics::collision::arithmetic
