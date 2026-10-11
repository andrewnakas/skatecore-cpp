// Port of skate-game/src/physics/air_phase/board.rs.
//!82D34AB0 consumes the completed Air reckoning's collision normal1216.
#pragma once
#include "s3/core/air/state.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::air_phase::board {

Status advance(GamePhysics& physics, SkaterRuntime& skater, ::s3::air::state::PhysicsAirFrame frame);

}  // namespace s3::game::physics::air_phase::board
