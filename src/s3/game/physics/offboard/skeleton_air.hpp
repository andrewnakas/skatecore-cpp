// Port of skate-game/src/physics/offboard/skeleton_air.rs
//! BipedAir Skeleton82BDDD70 with the existing skeleton, drives and solver.
//! The caller supplies the shared reckoning service; no second physics owner.
//!
//! `impl SkeletonInputRuntime { fn update_biped_air<F>(..) }` lives in this file in Rust;
//! SkeletonInputRuntime (physics/skeleton_input_runtime.hpp, group D) must declare
//!     rs::Result<offboard::skeleton_air::Frame, std::string> update_biped_air(
//!         skeleton_air::SkeletonAir& air, sc::physics::board_runtime::BoardRuntime& board,
//!         offboard::skeleton_air::Input input, sc::player::input_phase::ProcessedPhysicsInput& p,
//!         SkeletonOwners& owners, sc::physics::Slice<const sc::animation::output::NativeMatrix> globals,
//!         const CollisionInput& collision, sc::physics::rigid_body::RetailSimulationStep simulation,
//!         const offboard::skeleton_air::FinishReckoning& finish_reckoning);
//! and skeleton_air.cpp (this module) defines it.
#pragma once
#include <array>
#include <functional>

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/offboard/skeleton_ground.hpp"

namespace s3::game::physics::offboard::skeleton_air {

using Frame = sc::physics::skeleton_animation_record::AnimationPartTransform;
using skeleton_ground::ReckoningUpdate;

struct Input {
    Frame frame_208{};
    std::array<float, 4> trajectory_position_272{};
    std::array<float, 4> body_target_416{};
    float lift_436 = 0.0f;
    ///Actual processed board-forward96, NOT effective animation-forward224.
    std::array<float, 4> board_forward_96{};
};

/// `F: FnOnce(ReckoningUpdate, &ProcessedPhysicsInput, f32) -> Result<(), String>`.
using FinishReckoning =
    std::function<Status(ReckoningUpdate, const sc::player::input_phase::ProcessedPhysicsInput&, float)>;

}  // namespace s3::game::physics::offboard::skeleton_air
