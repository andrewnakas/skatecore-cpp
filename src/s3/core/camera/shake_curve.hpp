// Port of skate-core/src/camera/shake_curve.rs (pub(super)).
//! Camera shake Bezier inversion8296F530 and evaluation8296F388.
//! Basis initialized82F82738 from8232FB00/FAF0/FAE0/F660.
#pragma once
#include "s3/core/camera/_types.hpp"

namespace s3::camera::shake_curve {

float sample(const F4x4& points, float input);

}  // namespace s3::camera::shake_curve
