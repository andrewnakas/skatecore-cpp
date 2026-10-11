// Port of skate-game/src/physics/grind/tip_nudge.rs.
//! S3 tipslide dropping-in nudge82D421B0, separate from common exit nudge.
//! Scalar reconstruction of original operand flow; ordinary PC floating point,
//! not a claim of bit-exact VMX128 execution or an instruction emulator.
#pragma once
#include <array>

#include "s3/core/physics/board_runtime.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::grind::tip_nudge {

Status apply(::s3::physics::board_runtime::BoardRuntime& board, std::array<float, 4> position,
             std::array<float, 4> point, std::array<float, 4> across);

}  // namespace s3::game::physics::grind::tip_nudge
