// Port of skate-game/src/physics/grind/substate/skeleton.rs.
//!82D40D28 calls82BDDA10 with fast_blend=false only for a pop THIS update.
#pragma once
#include <string>

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::grind::substate::skeleton {

using ::s3::physics::skeleton_animation_record::AnimationPartTransform;

Status animated(GamePhysics& physics, SkaterRuntime& skater);

rs::Result<AnimationPartTransform, std::string> animated_target(GamePhysics& physics, SkaterRuntime& skater);

///Nonspecific performs82C04368 in its subsequent board helper, after steering.
rs::Result<AnimationPartTransform, std::string> ground_target(const GamePhysics& physics, SkaterRuntime& skater);

}  // namespace s3::game::physics::grind::substate::skeleton
