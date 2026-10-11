// Port of skate-game/src/physics/grind/involuntary.rs.
//! Native family substate2 dispatch; no generic tipslide substitution.
#pragma once
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"
#include "s3/game/physics/grind/observation.hpp"

namespace s3::game::physics::grind::involuntary {

using observation::ManagerObservation;

Status advance(GamePhysics& physics, SkaterRuntime& skater, const ManagerObservation& manager);

}  // namespace s3::game::physics::grind::involuntary
