// Port of skate-game/src/physics/grind/substate/nonspecific.rs.
//!82D42DF0 through board helper82D42F50; caller owns final grind reckoning.
#pragma once
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::grind::substate::nonspecific {

Status execute(GamePhysics& physics, SkaterRuntime& skater);

}  // namespace s3::game::physics::grind::substate::nonspecific
