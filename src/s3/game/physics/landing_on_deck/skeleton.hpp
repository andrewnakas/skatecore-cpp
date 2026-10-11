// Port of skate-game/src/physics/landing_on_deck/skeleton.rs
//! Skeleton82BDE428: old-root COM, new root, shared drives, shared reckoning.
#pragma once
#include <array>

#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::landing_on_deck::skeleton {

Status update(GamePhysics& physics, SkaterRuntime& skater, std::array<float, 4> position);

}  // namespace s3::game::physics::landing_on_deck::skeleton
