// Port of skate-core/src/player/wipeout_state/drives.rs (whole file).
// Original SkeletonDrives::UpdateRagdoll82BEB0C8 and root softness82BEB790.
#pragma once
#include <array>

#include "s3/core/physics/skeleton_body.hpp"

namespace s3::player::wipeout_state::drives {

using physics::skeleton_animation_record::AnimationPartTransform;

struct Settings {
    ///Original SkeletonBody748+36*part: start, normal, controlled, end, extra.
    std::array<std::array<float, 5>, 24> bone{};
    ///The four full64 root_drive_* stock scalars, in the same first-four order.
    std::array<float, 4> root{};
    ///animation/default: layout424 DriveStrengthLocal,420 DriveStrengthRootLocal.
    std::array<float, 2> strength{};
    ///physics_animation/default HookSoftDsp888, HookSoftStr884, HookSoftDmp892.
    float hook_spring = 0.0f;
    float hook_strength = 0.0f;
    float hook_damping = 0.0f;
};

struct Weights {
    float start = 0.0f;
    float end = 0.0f;
    float controlled = 0.0f;
    ///Processed2484 bits5 and4 select the two independently supplied weights.
    float upper_extra = 0.0f;
    float lower_extra = 0.0f;
};

///Returns original retained residual16. It is not clamped or renormalized.
float update(physics::skeleton_body::SkeletonDrives& drives, const std::array<AnimationPartTransform, 24>& pose,
             const Settings& settings, Weights weights);
///82BEB790 changes only target0's linear drive; damping uses the unscaled spring.
void set_linear_root(physics::skeleton_body::SkeletonDrives& drives, const Settings& s, float weight);
///82D3BE98..BF24 uses the same target0 angular fields set during Enter.
///Enter uses spring*0.5 and strength*1799.9998; Update uses the supplied weight.
void set_angular_root(physics::skeleton_body::SkeletonDrives& drives, const Settings& s, float weight);

}  // namespace s3::player::wipeout_state::drives
