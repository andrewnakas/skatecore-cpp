// Port of skate-core/src/player/wipeout_state/orientation.rs (whole file).
// Original Wipeout alignment82D3C770: Euler X/Y degrees -> rotated up axis.
#pragma once
#include "s3/core/player/wipeout_state/math.hpp"

namespace s3::player::wipeout_state::orientation {

using math::V;

V alignment_up(float x_degrees, float y_degrees);
///82D3C8CC..C8F8 and82D3CF84..CFFC retain the two fused cross stages.
V rotate(V quaternion, V vector);
///8296EC98: both vectors require squared length >0.0001, then one
///reciprocal-square-root refinement. The negative half returns 2pi-angle.
float signed_angle(V left, V right, V axis);
///8286CD88 checks the original lengths before projecting onto the axis plane.
float projected_angle(V left, V right, V axis);

}  // namespace s3::player::wipeout_state::orientation
