// Port of skate-core/src/physics/grind_contact/arithmetic.rs.
// Refinement sequence used by S3 82D861D8/82D88518/82D886B8.
// Seeds use existing independent PC math, not a Xenon bit-exact claim.
#pragma once
#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::physics::grind_contact::arithmetic {

inline float reciprocal(float value) {
    float result = native_arithmetic::reciprocal_estimate(value);
    for (int i = 0; i < 2; ++i) {
        const float error = rs::mul_add(-result, value, 1.0f);
        result = rs::mul_add(result, error, result);
    }
    return result;
}

inline float inverse_square_root(float value) {
    float result = native_arithmetic::reciprocal_square_root_estimate(value);
    for (int i = 0; i < 2; ++i) {
        const float square = result * result;
        const float half = result * 0.5f;
        const float error = rs::mul_add(-value, square, 1.0f);
        result = rs::mul_add(half, error, result);
    }
    return result;
}

inline float square_root(float value) {
    const float result = value * inverse_square_root(value);
    // The original vector select explicitly chooses zero for a zero radicand.
    return value == 0.0f ? 0.0f : result;
}

}  // namespace s3::physics::grind_contact::arithmetic
