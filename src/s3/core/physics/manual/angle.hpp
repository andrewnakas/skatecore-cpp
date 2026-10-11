// Port of skate-core/src/physics/manual/angle.rs.
// Finite scalar Angle::Normalize 8258DB98, from direct TU3 disassembly.
// Kept local pending coordinator's shared angle-operation audit.
#pragma once
#include "s3/core/rs.hpp"

namespace s3::physics::manual::angle {

enum class AngleError {
    /// Native fctiwz result for nonfinite/overflow input is not reconstructed.
    IntegerConversionUnavailable,
};

rs::Result<float, AngleError> normalize(float angle);

}  // namespace s3::physics::manual::angle
