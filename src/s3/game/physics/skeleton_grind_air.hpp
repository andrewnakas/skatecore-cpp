// Port of skate-game/src/physics/skeleton_grind_air.rs.
//!82BDCFB0 at the existing ProcessData boundary; never a physical teleport.
#pragma once
#include <string>
#include <vector>

#include "s3/core/animation/output/native.hpp"
#include "s3/core/physics/grind_air.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::animated_skeleton {
struct AnimatedSkeleton;  // physics/animated_skeleton.hpp (group D)
}

namespace s3::game::physics::skeleton_grind_air {

/// `globals: &[NativeMatrix]`.
rs::Result<bool, std::string> update(::s3::physics::grind_air::GrindAir& state,
                                     const ::s3::physics::grind_air::Settings& settings,
                                     ::s3::physics::skeleton_animation_record::AnimationPartTransform processed_deck,
                                     const ::s3::player::input_phase::ProcessedPhysicsInput& p,
                                     animated_skeleton::AnimatedSkeleton& animated,
                                     const std::vector<::s3::animation::output::NativeMatrix>& globals);

}  // namespace s3::game::physics::skeleton_grind_air
