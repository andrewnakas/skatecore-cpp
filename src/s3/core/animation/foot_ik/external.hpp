// Port of skate-core/src/animation/foot_ik/external.rs.
// SkeletonIK::CalculateExternalIKOffset82BEE390.
#pragma once
#include <array>

#include "s3/core/animation/foot_ik/status.hpp"
#include "s3/core/animation/foot_ik/transforms.hpp"

namespace s3::animation::foot_ik::external {

using Transform = physics::skeleton_animation_record::AnimationPartTransform;

/// `ExternalTarget{}` == Rust `Default`.
struct ExternalTarget {
    /// SkeletonIK544 /608; setters distinguish the supplied coordinate space.
    std::array<float, 4> world_position{};
    std::array<float, 4> animation_position{};
    /// SkeletonIK736,800,828. The normal is measured against world up.
    std::array<float, 4> normal{};
    bool normal_set = false;
    float normal_blend = 0.0f;
};

void update(ExternalTarget& target, status::LimbStatus& status, transforms::LimbFrames& frames,
            transforms::LimbBinding binding, const std::array<Transform, 24>& animation,
            const Transform& animation_to_world, const Transform& world_to_animation);

}  // namespace s3::animation::foot_ik::external
