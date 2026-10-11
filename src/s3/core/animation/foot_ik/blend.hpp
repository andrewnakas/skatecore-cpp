// Port of skate-core/src/animation/foot_ik/blend.rs.
// SkeletonIK::BlendTransforms82BEEEB8.
#pragma once
#include <array>

#include "s3/core/animation/foot_ik/status.hpp"
#include "s3/core/animation/foot_ik/transforms.hpp"

namespace s3::animation::foot_ik::blend {

using Transform = physics::skeleton_animation_record::AnimationPartTransform;

void update(const std::array<status::LimbStatus, 4>& statuses, std::array<transforms::LimbFrames, 4>& frames,
            const std::array<transforms::LimbBinding, 4>& bindings, const Transform& physical_board,
            const Transform& animation_to_world, const Transform& animated_board);

}  // namespace s3::animation::foot_ik::blend
