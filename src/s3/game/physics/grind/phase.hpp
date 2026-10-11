// Port of skate-game/src/physics/grind/phase.rs.
//! CURRENT GamePhysics/SkaterRuntime adapters. Shared scheduling belongs to main.
#pragma once
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::grind::phase {

Status enter(GamePhysics& physics, SkaterRuntime& skater);
Status exit(GamePhysics& physics, SkaterRuntime& skater);
Status fill(const GamePhysics& physics, SkaterRuntime& skater);
Status post(const GamePhysics& physics, SkaterRuntime& skater);

}  // namespace s3::game::physics::grind::phase
