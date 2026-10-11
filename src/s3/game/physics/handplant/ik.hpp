// Port of skate-game/src/physics/handplant/ik.rs
//! HandPlantManager82D633B0 and Skeleton82BD9728/97D0, shared four-limb IK.
#pragma once
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::handplant::ik {

void update(SkaterRuntime& skater, bool ground);

}  // namespace s3::game::physics::handplant::ik
