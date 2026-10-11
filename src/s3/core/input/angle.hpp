// Port of skate-core/src/input/angle.rs (whole file).
// Scalar lane of native SIMD atan 82473B98 and intention left-stick angle.
#pragma once

namespace s3::input::angle {

/// pub(super) in Rust.
float reciprocal_estimate(float value);
float atan(float value);
/// pub(super) in Rust.
float left_stick_angle(float x, float y);

}  // namespace s3::input::angle
