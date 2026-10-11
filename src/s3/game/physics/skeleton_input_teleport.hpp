// Port of skate-game/src/physics/skeleton_input_teleport.rs
// (mounted as `skeleton_input_runtime::teleport` via #[path]).
//! Actual teleport skeleton stages82BE3508 and82BDF7C0. The player owns the
//! intervening ResetSystems and ProcessData calls; this is the same skeleton.
//
// This module only contains `impl SkeletonInputRuntime { reset_for_teleport,
// update_teleport }`, declared in skeleton_input_runtime.hpp and defined in
// skeleton_input_teleport.cpp.
#pragma once

namespace s3::game::physics::skeleton_input_runtime::teleport {}
