// Port of skate-game/src/physics/wipeout_states/skeleton.rs
//! Original Skeleton::UpdateWipeout82BDFA88 on the live physical owners.
#pragma once
#include <string>
#include <vector>

#include "s3/core/animation/output/native.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/core/player/wipeout_state.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/skeleton_input_runtime.hpp"

namespace s3::game::physics::wipeout_states::skeleton {

///The original r8 byte skips only target-position updates. It does not skip
///root construction, COM frames, first-state-tick IK, or ragdoll drive updates.
/// `globals: &[NativeMatrix]` -> pointer + length.
rs::Result<float, std::string> update(skeleton_input_runtime::SkeletonInputRuntime& input,
                                      skeleton_input_runtime::SkeletonOwners& owners,
                                      const ::s3::player::input_phase::ProcessedPhysicsInput& processed,
                                      const ::s3::animation::output::NativeMatrix* globals, std::size_t globals_len,
                                      const ::s3::player::wipeout_state::drives::Settings& settings, float start,
                                      float end, float controlled, float extra, bool skip_targets);

}  // namespace s3::game::physics::wipeout_states::skeleton
