// Port of skate-core/src/camera/manager_incline.rs (pub(super) helpers; the
// ManagerState::update_incline method is declared in manager_state.hpp).
//! Ground incline and frontside state, complete TU3 82E002A8.
#pragma once
#include "s3/core/camera/_types.hpp"

namespace s3::camera::manager_incline {

float angle_between(F4 a, F4 b);

/// Inline atan wrapper82E005A4..82E00628: one reciprocal Newton step,
/// multiply-add with positive zero, then signed quadrant selection.
float atan_ratio(float numerator, float denominator);

}  // namespace s3::camera::manager_incline
