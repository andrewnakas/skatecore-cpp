// Port of skate-game/src/physics/player_state/transition.rs.
//! Native SetPhysicsState82DB8540 publication, followed by owned Enter/Exit.
//! (The private `Calls` PhysicalStateCalls adapter lives in transition.cpp.)
#pragma once
#include "s3/core/player/state.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::player_state::transition {

/// `pub(super)`.
Status set(GamePhysics& physics, SkaterRuntime& skater, sc::player::state::PhysicalStateId requested);

}  // namespace s3::game::physics::player_state::transition
