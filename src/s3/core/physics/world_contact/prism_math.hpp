// Port of skate-core/src/physics/world_contact/prism_math.rs.
#pragma once
#include "s3/core/physics/reciprocal_sqrt.hpp"
#include "s3/core/physics/world_contact/arithmetic.hpp"

namespace s3::physics::world_contact::prism_math {

using V = std::array<float, 4>;

inline V load(const std::uint32_t* words, std::size_t offset) {
    return V{rs::from_bits(words[offset]), rs::from_bits(words[offset + 1]),
             rs::from_bits(words[offset + 2]), rs::from_bits(words[offset + 3])};
}
template <std::size_t N>
inline V load(const std::array<std::uint32_t, N>& words, std::size_t offset) {
    return load(words.data(), offset);
}

inline float dot(V a, V b) { return arithmetic::dot({a[0], a[1], a[2]}, {b[0], b[1], b[2]}); }

inline V sub(V a, V b) { return V{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }

inline V scale(V a, float s) { return V{a[0] * s, a[1] * s, a[2] * s, a[3] * s}; }

inline V madd(V a, float s, V b) {
    return V{rs::mul_add(a[0], s, b[0]), rs::mul_add(a[1], s, b[1]), rs::mul_add(a[2], s, b[2]),
             rs::mul_add(a[3], s, b[3])};
}

inline V cross(V a, V b) {
    return V{
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
        rs::mul_add(-a[3], b[3], a[3] * b[3]),
    };
}

inline float inverse_length(float squared) {
    float r = reciprocal_sqrt::estimate(squared);
    for (int i = 0; i < 2; ++i) {
        r = rs::mul_add(r * 0.5f, rs::mul_add(-squared, r * r, 1.0f), r);
    }
    return r;
}

inline V normalized(V a) {
    const float squared = dot(a, a);
    return scale(a, inverse_length(squared));
}

inline float reciprocal_estimate(float value) { return native_arithmetic::reciprocal_estimate(value); }

inline float reciprocal(float value) {
    float r = reciprocal_estimate(value);
    for (int i = 0; i < 2; ++i) {
        r = rs::mul_add(r, rs::mul_add(-r, value, 1.0f), r);
    }
    return r;
}

/// `v.map(f32::to_bits)` for a four-lane vector.
inline std::array<std::uint32_t, 4> bits(V v) { return rs::to_bits(v); }
/// Copies four lanes into `words[offset..offset + 4]`.
inline void store_bits(std::uint32_t* words, std::size_t offset, V v) {
    for (std::size_t i = 0; i < 4; ++i) words[offset + i] = rs::to_bits(v[i]);
}

}  // namespace s3::physics::world_contact::prism_math
