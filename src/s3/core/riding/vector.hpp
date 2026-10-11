// Port of skate-core/src/riding/vector.rs.
// Module-private (`mod vector`) helpers shared by the riding toolkit leaves.
#pragma once
#include <array>
#include <cstddef>

#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/reciprocal_sqrt.hpp"

namespace s3::riding::vector {

inline float dot3(const std::array<float, 4>& a, const std::array<float, 4>& b) {
    return physics::native_arithmetic::dot3(a, b);
}

/// Inline NormalizeSafe sequence in TU3 82C04F68 / 82D92CF8.
inline std::array<float, 4> normalize(std::array<float, 4> v, std::array<float, 4> threshold) {
    const float squared = dot3(v, v);
    float inverse = physics::reciprocal_sqrt::estimate(squared);
    for (int n = 0; n < 2; ++n) {
        const float correction = rs::mul_add(-squared, inverse * inverse, 1.0f);
        inverse = rs::mul_add(inverse * 0.5f, correction, inverse);
    }
    const float length = squared == 0.0f ? 0.0f : squared * inverse;
    std::array<float, 4> out{};
    for (std::size_t i = 0; i < 4; ++i) {
        out[i] = length > threshold[i] ? v[i] * inverse : 0.0f;
    }
    return out;
}

inline float clamp(float value, float lower, float upper) {
    const float v = lower - value >= 0.0f ? lower : value;
    return upper - v >= 0.0f ? v : upper;
}

inline std::array<float, 4> cross(const std::array<float, 4>& a, const std::array<float, 4>& b) {
    std::array<float, 4> out{};
    for (std::size_t i = 0; i < 4; ++i) {
        const std::size_t j = i == 3 ? 3 : (i + 1) % 3;
        const std::size_t k = i == 3 ? 3 : (i + 2) % 3;
        out[i] = rs::mul_add(-a[k], b[j], a[j] * b[k]);
    }
    return out;
}

}  // namespace s3::riding::vector
