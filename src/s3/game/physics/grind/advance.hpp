// Port of skate-game/src/physics/grind/advance.rs. (C++ namespace `grind::advance_`: the re-exported fn `grind::advance` takes the plain name)
//! Native common82D3F4E8 and Nonspecific82D42DF0 Update order.
#pragma once
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::grind::advance_ {

Status advance(GamePhysics& physics, SkaterRuntime& skater);

}  // namespace s3::game::physics::grind::advance_
