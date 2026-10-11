// Port of skate-game/src/physics/wipeout_states/update.rs
//! Complete Wipeout300 update order82D3B9C8, on the game body/pose owners.
#pragma once
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::wipeout_states::update {

Status advance(GamePhysics& physics, SkaterRuntime& skater);

}  // namespace s3::game::physics::wipeout_states::update
