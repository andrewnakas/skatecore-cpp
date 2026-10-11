// Port of skate-game/src/physics/handplant/rotation.rs
//! CreateMtxFromHeadingAndUp82D61B60 and axis-angle frame blend82BD3150.
#pragma once
#include <array>

#include "s3/game/common.hpp"

namespace s3::game::physics::handplant::rotation {

using V = std::array<float, 4>;  // super::V

std::array<V, 4> frame(V heading, V up);
std::array<V, 4> blend(std::array<V, 4> a, std::array<V, 4> b, float weight);

}  // namespace s3::game::physics::handplant::rotation
