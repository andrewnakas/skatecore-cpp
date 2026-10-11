// Port of skate-core/src/physics/grind_forces/orientation.rs.
// ZIP orientation formulas checked against original S3 family bodies.
// S2 equivalents82D9B818/918/BAC0/BCF8/BD88 retain tunable angles/blends;
// S3 uses the literals below and adds darkslide's inverted normal.
#pragma once
#include "s3/core/physics/grind_forces.hpp"

namespace s3::physics::grind_forces::orientation {

using Frame = std::array<V, 4>;

struct Input {
    /// Exact Processed64..127, not an effective/fakie-adjusted board frame.
    Frame board{};
    V direction{};
    V normal{};
    V point{};
    float yaw_1504 = 0.0f;
    float pitch_1508 = 0.0f;
    bool switched = false;
};

struct Target {
    Frame frame{};
    float noise_amount = 0.0f;
};

rs::Result<Target, const char*> target(std::uint32_t family, Input i);

/// 82D40890: reverse-order classical Gram-Schmidt on BOTH frames, then
/// CURRENT's original82BD3150 helper. The temporary orthonormal frames' fourth
/// vectors are explicitly zeroed (82D40930/82D409C8), not a position teleport.
Frame blend(Frame current, Frame target);

}  // namespace s3::physics::grind_forces::orientation
