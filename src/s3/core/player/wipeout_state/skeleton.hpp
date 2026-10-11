// Port of skate-core/src/player/wipeout_state/skeleton.rs (whole file).
// Original Skeleton::UpdateRootTransformsWipeout82BDFBE8.
#pragma once
#include <array>

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/physics/skeleton_root.hpp"

namespace s3::player::wipeout_state::skeleton {

///Reconstruct the root from the cached physical hips and animated hips,
///then blend its predicted position toward the actual weighted physical COM.
///This updates animation roots; it does not overwrite any solved body rates.
void update_roots(physics::skeleton_root::SkeletonRootFrames& roots,
                  const physics::skeleton_animation_record::AnimationPartTransform& animation_hips,
                  const physics::skeleton_animation_record::AnimationPartTransform& physical_hips,
                  std::array<float, 4> hips_velocity, std::array<float, 4> physical_com,
                  std::array<float, 4> com_velocity, float state_time);

}  // namespace s3::player::wipeout_state::skeleton
