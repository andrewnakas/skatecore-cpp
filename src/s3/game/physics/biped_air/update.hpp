// Port of skate-game/src/physics/biped_air/update.rs
//! Ordered82D2EF38, including actual skeleton/selector/landing consumers.
#pragma once
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::biped_air::update {

Status update(GamePhysics& physics, SkaterRuntime& skater, const PlayerControls& controls);

Status skeleton(GamePhysics& physics, SkaterRuntime& skater);

}  // namespace s3::game::physics::biped_air::update
