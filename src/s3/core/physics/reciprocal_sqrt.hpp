// Port of skate-core/src/physics/reciprocal_sqrt.rs.
// Compatibility entry point for recovered callers that use `vrsqrtefp`.
#pragma once
#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::physics::reciprocal_sqrt {

inline float estimate(float value) {
    return native_arithmetic::reciprocal_square_root_estimate(value);
}

}  // namespace s3::physics::reciprocal_sqrt
