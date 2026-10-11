// Port of skate-game/src/physics/player_state/pre_state.rs.
// NOTE: namespace is `pre_state_` (not `pre_state`) because the parent declares free fn `player_state::pre_state`.
//! Update3_State82DB6050 prefix through grab-spline query synchronization.
//! Ground/Air/KnownAir vtable24 ->82D34DA8 ->82D2D860 predicts from the actual deck part
//! transform and its physical body's linear velocity, with a single VMADD.
#pragma once
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::player_state::pre_state_ {

/// `pub(super)`.
Status advance(GamePhysics& physics, SkaterRuntime& skater);

}  // namespace s3::game::physics::player_state::pre_state_
