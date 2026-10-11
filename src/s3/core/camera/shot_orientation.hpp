// Port of skate-core/src/camera/shot_orientation.rs (pub(super)).
//! Quaternion interpolation82E07540, with the native sign choice and near-angle
//! branch. Shared Xenon arithmetic remains approximation-derived; no generated
//! implementation is linked or imported.
#pragma once
#include "s3/core/camera/_types.hpp"

namespace s3::camera::shot_orientation {

F4 interpolate_orientation(F4 from, F4 to, float fraction);

/// FrameComposer82E01740 uses the same interpolation without sine-easing its
/// input. Native negates the first quaternion when the dot product is negative.
F4 slerp(F4 from, F4 to, float weight);

}  // namespace s3::camera::shot_orientation
