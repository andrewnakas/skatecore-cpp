// Port of skate-game/src/physics/grind/contact.rs.
//! Six native substate1 dispatches. Manager query/family selection is upstream.
#pragma once
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"
#include "s3/game/physics/grind/observation.hpp"

namespace s3::game::physics::grind::contact {

using observation::ManagerObservation;

Status advance(GamePhysics& physics, SkaterRuntime& skater, const ManagerObservation& manager);

}  // namespace s3::game::physics::grind::contact
