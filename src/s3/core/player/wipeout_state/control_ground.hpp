// Port of skate-core/src/player/wipeout_state/control_ground.rs (whole file).
// Original Wipeout contact control82D3D660, using the world-space animation hands.
#pragma once
#include <array>

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/player/wipeout_state/data.hpp"
#include "s3/core/player/wipeout_state/math.hpp"
#include "s3/core/player/wipeout_state/profiles.hpp"

namespace s3::player::wipeout_state::control_ground {

void update(State& state, physics::skeleton_body::SkeletonBody& body, math::V physical_com,
            const physics::skeleton_animation_record::AnimationPartTransform& effective, math::V ground_axis_464,
            const profiles::Profile& profile,
            const physics::skeleton_animation_record::AnimationPartTransform& animation_to_world,
            const std::array<physics::skeleton_animation_record::AnimationPartTransform, 24>& animation_pose);

}  // namespace s3::player::wipeout_state::control_ground
