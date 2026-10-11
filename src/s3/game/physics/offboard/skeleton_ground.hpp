// Port of skate-game/src/physics/offboard/skeleton_ground.rs
//! Original Biped Skeleton update82BDE060 with the existing physical owners.
//!
//! `impl SkeletonInputRuntime { fn update_biped_ground<F>(..) }` lives in this file in Rust;
//! SkeletonInputRuntime (physics/skeleton_input_runtime.hpp, group D) must declare
//!     rs::Result<offboard::skeleton_ground::Transform, std::string> update_biped_ground(
//!         skeleton_air::SkeletonAir& air, sc::physics::board_runtime::BoardRuntime& board,
//!         offboard::skeleton_ground::Input input, offboard::skeleton_ground::State& state,
//!         sc::player::input_phase::ProcessedPhysicsInput& p, SkeletonOwners& owners,
//!         sc::physics::Slice<const sc::animation::output::NativeMatrix> globals,
//!         const CollisionInput& collision, sc::physics::rigid_body::RetailSimulationStep simulation,
//!         const std::function<Status(offboard::skeleton_ground::ReckoningUpdate)>& finish_reckoning);
//! and skeleton_ground.cpp defines it.
#pragma once
#include <array>
#include <functional>

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::offboard::skeleton_ground {

using Transform = sc::physics::skeleton_animation_record::AnimationPartTransform;

///Actual Sync input: caller's constructed frame and Biped state1056 COM.
///State retains animation-space16016 independently of world-space12496.
struct Input {
    const Transform* world_frame = nullptr;
    std::array<float, 4> centre_of_mass_1056{};
};
///Terminal82D8E3E0 arguments, emitted after GeneralUpdate and trajectory clear.
///The caller invokes the canonical ground_reckoning producer in this callback.
struct ReckoningUpdate {
    std::array<float, 4> up{};
    std::array<float, 4> forward{};
    float blend = 0.0f;
};

/// `F: FnOnce(ReckoningUpdate) -> Result<(), String>`.
using FinishReckoning = std::function<Status(ReckoningUpdate)>;

}  // namespace s3::game::physics::offboard::skeleton_ground

#include "s3/game/physics/offboard/skeleton_ground/frames.hpp"
#include "s3/game/physics/offboard/skeleton_ground/state.hpp"

namespace s3::game::physics::offboard::skeleton_ground {
namespace state {
struct State;
}
using state::State;
// #[cfg(test)] pub(crate) use pose_audit::audit_render_parts; -> tests only.
}  // namespace s3::game::physics::offboard::skeleton_ground
