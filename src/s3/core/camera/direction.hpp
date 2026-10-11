// Port of skate-core/src/camera/direction.rs.
//! Complete TU3 8258DC10 direction-to-angular-vector operation, shared by
//! normal rig framing and avoidance. Input Y is consumed directly, not normalized.
#pragma once
#include "s3/core/camera/_types.hpp"

namespace s3::camera {

F4 direction_to_angles(F4 direction);

}  // namespace s3::camera
