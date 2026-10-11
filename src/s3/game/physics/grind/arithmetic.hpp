// Port of skate-game/src/physics/grind/arithmetic.rs.
//! Same scalar dot ordering as CURRENT core's private native arithmetic helper.
#pragma once
#include <array>

namespace s3::game::physics::grind::arithmetic {

inline float dot3(std::array<float, 4> a, std::array<float, 4> b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

}  // namespace s3::game::physics::grind::arithmetic
