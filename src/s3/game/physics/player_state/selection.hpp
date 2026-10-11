// Port of skate-game/src/physics/player_state/selection.rs.
//! CalcSuggestedState82D8ADE8 gets actual contacts, line distances and pose.
#pragma once
#include "s3/core/player/input_phase.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::player_state::selection {

/// `pub(super)`.
Status advance(GamePhysics& physics, SkaterRuntime& skater,
               sc::player::input_phase::ProcessedPhysicsSnapshot snapshot);

}  // namespace s3::game::physics::player_state::selection
