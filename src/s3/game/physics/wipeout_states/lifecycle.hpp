// Port of skate-game/src/physics/wipeout_states/lifecycle.rs
//! Original Wipeout Enter82D3B5E8 and Exit82D3B990.
#pragma once
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::wipeout_states::lifecycle {

Status enter(GamePhysics& physics, SkaterRuntime& skater);
void exit(GamePhysics& physics, SkaterRuntime& skater);
void post_physics(SkaterRuntime& skater);

}  // namespace s3::game::physics::wipeout_states::lifecycle
