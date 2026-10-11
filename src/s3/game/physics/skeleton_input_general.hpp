// Port of skate-game/src/physics/skeleton_input_general.rs
// (mounted as `skeleton_input_runtime::general` via #[path]).
//! Movement-phase continuation of the original Skeleton input owner.
//
// `impl SkeletonInputRuntime { update_ground, general_update, reset_physical_pose }` is
// declared in skeleton_input_runtime.hpp and defined in skeleton_input_general.cpp.
#pragma once
#include "s3/core/physics/skeleton_body.hpp"

namespace s3::game::physics::skeleton_input_runtime::general {

/// pub(super)
void restore_board_target_drives(::s3::physics::skeleton_body::SkeletonDrives& drives);

}  // namespace s3::game::physics::skeleton_input_runtime::general
