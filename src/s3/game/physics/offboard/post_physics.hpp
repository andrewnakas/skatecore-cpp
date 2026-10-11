// Port of skate-game/src/physics/offboard/post_physics.rs
//! Offboard state slot40 at82DB6DCC, after Skeleton feedback82DB6DB8 and
//! before PostWipeoutCheck82DB6DF8 consumes the same request owner.
#pragma once
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::offboard::post_physics {

Status advance(const GamePhysics& physics, SkaterRuntime& skater);

}  // namespace s3::game::physics::offboard::post_physics
