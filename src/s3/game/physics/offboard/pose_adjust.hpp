// Port of skate-game/src/physics/offboard/pose_adjust.rs
//! Original82BD8F10 writes the existing AnimatedSkeleton board-offset owner.
#pragma once
#include <array>
#include <cstddef>

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/physics/slice.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/core/player/offboard/pose_adjust.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::animated_skeleton {
struct AnimatedSkeleton;  // physics/animated_skeleton.hpp (group D)
}

namespace s3::game::physics::offboard::pose_adjust {

/// Reparented indices are the existing target_bones[2] and[3] loaded by82BED688.
/// No state is changed when category500/state503 eligibility is false.
Status update(animated_skeleton::AnimatedSkeleton& animated,
              sc::physics::Slice<const sc::physics::skeleton_animation_record::AnimationPartTransform> globals,
              std::array<std::size_t, 2> reparented_hands,
              const sc::player::input_phase::ProcessedPhysicsInput& processed);

}  // namespace s3::game::physics::offboard::pose_adjust
