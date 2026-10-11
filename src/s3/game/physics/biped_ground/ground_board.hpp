// Port of skate-game/src/physics/biped_ground/ground_board.rs
//! Ground carried-board Update82D31D1C..DB0. State60 is SkateboardController;
//! state52 in Sync82D324B0 is a DIFFERENT grab-spline manager.
#pragma once
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::biped_ground::ground_board {

/// Executes original Stop/Hold effects on the existing possession/solver owner.
/// Does not run its common per-frame Update a second time.
void update_possession(GamePhysics& physics, SkaterRuntime& skater);

}  // namespace s3::game::physics::biped_ground::ground_board
