// Port of skate-core/src/physics/world_contact/arithmetic.rs.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::physics::world_contact {
// Type aliases from world_contact/mod.rs, declared here so that submodule headers
// need not include the parent module header.

/// Maximum-feature record consumed by 82ACE190.
using MaximumFeature = std::array<std::uint32_t, 144>;
/// Native two point arrays, normal, count, flags and untouched trailing padding.
using FeaturePrism = std::array<std::uint32_t, 136>;
/// Native 256-byte contact record, including both body workspaces.
using ContactRecord = std::array<std::uint32_t, 64>;
}  // namespace s3::physics::world_contact

namespace s3::physics::world_contact::arithmetic {

using V = std::array<float, 3>;

inline V vector(const std::uint32_t* record, std::size_t offset) {
    return V{rs::from_bits(record[offset]), rs::from_bits(record[offset + 1]),
             rs::from_bits(record[offset + 2])};
}
template <std::size_t N>
inline V vector(const std::array<std::uint32_t, N>& record, std::size_t offset) {
    return vector(record.data(), offset);
}

inline V sub(V a, V b) { return V{a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }

inline float dot(V a, V b) {
    return native_arithmetic::dot3({a[0], a[1], a[2], 0.0f}, {b[0], b[1], b[2], 0.0f});
}

inline V cross(V a, V b) {
    return V{
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
    };
}

inline V scale(V a, float b) { return V{a[0] * b, a[1] * b, a[2] * b}; }

}  // namespace s3::physics::world_contact::arithmetic
