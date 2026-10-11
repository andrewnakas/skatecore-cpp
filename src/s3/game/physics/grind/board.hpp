// Port of skate-game/src/physics/grind/board.rs.
//! Concrete CURRENT BoardRuntime adaptation of AddWorldForce82C03F98.
#pragma once
#include <array>

#include "s3/core/physics/assembly.hpp"
#include "s3/core/physics/board_runtime.hpp"

namespace s3::game::physics::grind::board {

/// Immediate deck acceleration/torque accumulation. Unlike queued board forces,
/// this has no capacity, attribution tag or stock force-point Y offset.
void apply_world_force(::s3::physics::board_runtime::BoardRuntime& board, std::array<float, 4> force,
                       std::array<float, 4> point);

void apply_to_deck(::s3::physics::assembly::BodySnapshot& deck, std::array<float, 4> force,
                   std::array<float, 4> point);

}  // namespace s3::game::physics::grind::board
