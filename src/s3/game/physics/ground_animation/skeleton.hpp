// Port of skate-game/src/physics/ground_animation/skeleton.rs.
//!82D33E00 calls SkeletonUpdateAnimated82BDDA10 with fast blending enabled.
#pragma once
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::ground_animation::skeleton {

/// `pub(super)`.
Status advance(GamePhysics& physics, SkaterRuntime& skater);

}  // namespace s3::game::physics::ground_animation::skeleton
