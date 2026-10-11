// Port of skate-core/src/physics/native_arithmetic.rs.
//
// Independent host math for the operations used by recovered TU3 callers.
// Uses ordinary floating-point operations without external estimate tables or
// translator-specific special cases. Callers retain their recovered refinement
// steps. This is working PC math, not bit-exact Xenon instruction emulation.
#pragma once
#include <array>

#include "s3/core/rs.hpp"

namespace s3::physics::native_arithmetic {

inline float reciprocal_estimate(float value) { return rs::recip(value); }

inline float reciprocal_square_root_estimate(float value) { return rs::recip(rs::sqrt(value)); }

inline float dot3(const std::array<float, 4>& left, const std::array<float, 4>& right) {
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
}

inline float dot4(const std::array<float, 4>& left, const std::array<float, 4>& right) {
    return dot3(left, right) + left[3] * right[3];
}

inline float vector_min(float left, float right) { return rs::min(left, right); }

inline float vector_max(float left, float right) { return rs::max(left, right); }

}  // namespace s3::physics::native_arithmetic
