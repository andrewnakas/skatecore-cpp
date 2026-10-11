// Port of skate-game/src/physics/skeleton_input_runtime.rs.
//! The production Skeleton::ProcessData82BD8918 boundary. All borrows point
//! at the same owners subsequently used by Ground, the solve and pose output.
//! GeneralUpdate belongs to the later movement-state phase, not PlayerInput.
//
// SkeletonInputRuntime has `impl` blocks in several Rust files; every method is
// declared here and defined in the matching .cpp:
//   skeleton_input_runtime.cpp  load, process_data
//   skeleton_input_general.cpp  update_ground, general_update, reset_physical_pose
//   skeleton_input_teleport.cpp reset_for_teleport, update_teleport
//   skeleton_air.cpp            update_animated, update_known_air
//   offboard/skeleton_air.cpp   update_biped_air      (group A)
//   offboard/skeleton_ground.cpp update_biped_ground  (group A)
#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/output/native.hpp"
#include "s3/core/animation/output/physics_packet.hpp"
#include "s3/core/animation/slice.hpp"
#include "s3/core/input/controller.hpp"
#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/board_toolkit.hpp"
#include "s3/core/physics/grind_air.hpp"
#include "s3/core/physics/rigid_body.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/physics/skeleton_output/correction.hpp"
#include "s3/core/physics/skeleton_output/wobble.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/foot_ik.hpp"

namespace s3::game::physics {
namespace animated_skeleton {
struct AnimatedSkeleton;
}
namespace animation_input {
struct AnimationInput;
}
namespace skeleton_air {
struct SkeletonAir;
}
// Group A (physics/offboard/skeleton_air.rs, physics/offboard/skeleton_ground.rs).
namespace offboard::skeleton_air {
struct Input;
}
namespace offboard::skeleton_ground {
struct Input;
struct ReckoningUpdate;
namespace state {
struct State;
}
}  // namespace offboard::skeleton_ground
}  // namespace s3::game::physics

namespace s3::game::physics::skeleton_input_runtime {

using Transform = sc::physics::skeleton_animation_record::AnimationPartTransform;
using sc::animation::output::NativeMatrix;
using sc::animation::output::attributes::AnimationAttribute;
using sc::player::input_phase::ProcessedPhysicsInput;

/// Original SkeletonCollision publications. The collision owner fills these
/// from actual solved contacts; there is deliberately no default observation.
struct CollisionInput {
    bool contact_4070 = false;
    bool has_pose_error_4077 = false;
    std::array<float, 4> pose_error_16272{};
    bool partial_ragdoll = false;
    float drive_weight_4028 = 0.0f;
};

/// Rust `SkeletonOwners<'a>`: all `&'a mut` borrows.
struct SkeletonOwners {
    animated_skeleton::AnimatedSkeleton& animated;
    sc::physics::skeleton_body::SkeletonBody& body;
    sc::physics::skeleton_body::SkeletonDrives& drives;
    foot_ik::FootIk& ik;
    animation_input::AnimationInput& animation_input;
    sc::physics::skeleton_output::correction::CorrectionState& correction;
    sc::physics::skeleton_body::SkeletonPoseErrors& pose_errors;
};

/// Rust `SkeletonPoseInput<'a>`.
struct SkeletonPoseInput {
    sc::animation::Slice<const NativeMatrix> globals;
    sc::animation::Slice<const AnimationAttribute> attributes;
    sc::input::controller::ActionMap& actions;
};

namespace detail {
inline std::array<Transform, 24> identity_frames_24() {
    std::array<Transform, 24> frames;
    frames.fill(sc::physics::skeleton_animation_record::IDENTITY);
    return frames;
}
}  // namespace detail

/// `impl Default` -> member initialisers (Original Skeleton ctor82BD78xx/79xx zeroes
/// these vector histories; 82BD7A78/84/8C clear16504/16506/16508. Frames are native identity.)
struct SkeletonInputRuntime {
    ///Skeleton16128, read from the actual deck before ProcessAnimAttributes.
    std::array<float, 4> deck_velocity{};
    ///Skeleton16336 and16352, the animation root derivative and its history.
    std::array<float, 4> root_velocity{};
    std::array<float, 4> previous_root_position{};
    ///Skeleton12240 from the actual target update, not animation-to-world.
    Transform animation_board_to_physics = sc::physics::skeleton_animation_record::IDENTITY;
    ///Skeleton11632/11648/11664 from the three extra-target positions.
    std::array<std::array<float, 4>, 3> extra_target_positions{};
    ///Skeleton16208, authored hips translation cached by physical reset.
    std::array<float, 4> reset_local_hips{};
    ///Skeleton12624, rewritten by ProcessData then modified in place by IK.
    ///The animation COM record retains its pre-IK copy independently.
    std::array<Transform, 24> drive_frames = detail::identity_frames_24();
    bool reenable_requested = false;
    bool invalid_target_reset = false;
    bool teleporting = false;
    ///Skeleton16420, changed by graph force-mode requests and reset82BE3508.
    std::uint32_t force_mode = 0;
    ///HeadTracking32..144 and160. Reset82BE3550..35A4 preserves its settings.
    std::array<std::array<float, 4>, 8> head_tracking_history{};
    bool head_tracking_active = false;
    ///GrindAirAdjust ctor82D7116C initializes activated289=false.
    ///Only KnownAir Update82D35D30/Exit82D3597C subsequently write this flag.
    ///288, ctor82D71160; activated289 is a separate retained flag.
    bool grind_air_started = false;
    bool grind_air_active = false;
    bool grind_air_adjusting = false;
    sc::physics::grind_air::GrindAir grind_air{};
    // Private in Rust.
    std::optional<sc::physics::grind_air::Settings> grind_air_settings;

    static SkeletonInputRuntime default_() { return SkeletonInputRuntime{}; }

    // ---- skeleton_input_runtime.rs ----
    static rs::Result<SkeletonInputRuntime, std::string> load(const sd::Collections& data);
    ///Called by PlayerInputCallbacks::process_skeleton, after the actual
    ///BoardToolkit and line tests have been published. It does not advance
    ///Ground, rebuild a toolkit or submit a second physical simulation.
    Status process_data(sc::physics::board_runtime::BoardRuntime& board,
                        const sc::physics::board_toolkit::BoardToolkit& toolkit,
                        const sc::player::input_phase::AnimationInputPacket& packet,
                        sc::player::input_phase::PhysicalPlayerInput& physical, ProcessedPhysicsInput& p,
                        SkeletonOwners& owners, SkeletonPoseInput pose, const CollisionInput& collision);

    // ---- skeleton_input_general.rs ----
    ///Ground82BDF530: update actual roots/board frames, GeneralUpdate, then
    ///reset next trajectory. Return the native Processed0..48 publication.
    rs::Result<Transform, std::string> update_ground(const sc::physics::board_runtime::BoardRuntime& board,
                                                     const Transform& reckoning, ProcessedPhysicsInput& p,
                                                     SkeletonOwners& owners,
                                                     sc::animation::Slice<const NativeMatrix> globals,
                                                     const CollisionInput& collision,
                                                     sc::physics::rigid_body::RetailSimulationStep simulation);
    ///Source order: root derivative -> real target writes -> discontinuity
    ///reset -> IK -> bone drive update -> fixed-step gravity displacement.
    rs::Result<foot_ik::DriveOutput, std::string> general_update(
        const ProcessedPhysicsInput& p, SkeletonOwners& owners, sc::animation::Slice<const NativeMatrix> globals,
        const CollisionInput& collision, sc::physics::rigid_body::RetailSimulationStep simulation);
    ///Real82BE2798 reset is also callable by the coordinated player reset;
    ///it clears correction pending but does not invent a fresh state machine.
    void reset_physical_pose(SkeletonOwners& owners, sc::physics::rigid_body::RetailSimulationStep simulation);

    // ---- skeleton_input_teleport.rs ----
    ///82BE3508, after board reset and before player ResetSystems82DB92D0.
    ///Does not replace owners with startup instances or reset unrelated rates.
    void reset_for_teleport(SkeletonOwners& owners, sc::physics::skeleton_body::SkeletonCollisionMode& mode,
                            sc::physics::skeleton_body::SkeletonCollisionFeedback& feedback,
                            sc::physics::skeleton_output::wobble::Wobble& wobble, bool& ground_elapsed_16505);
    ///82DB8998 brackets82BDF7C0 with Skeleton16508=true/false after the real
    ///ProcessData call. Reckoning816 is its freshly reset original frame.
    rs::Result<Transform, std::string> update_teleport(const sc::physics::board_runtime::BoardRuntime& board,
                                                       const Transform& reckoning_frame_816, ProcessedPhysicsInput& p,
                                                       SkeletonOwners& owners,
                                                       sc::animation::Slice<const NativeMatrix> globals,
                                                       const CollisionInput& collision,
                                                       sc::physics::rigid_body::RetailSimulationStep simulation);

    // ---- skeleton_air.rs ----
    ///82BDDA10. PhysicsAir passes fast=false; GroundAnimation passes true.
    ///Returns Processed0..48 / Skeleton15952, the UNBLENDED animation target.
    ///The effective blended matrix is published to Skeleton12496 separately.
    rs::Result<Transform, std::string> update_animated(skeleton_air::SkeletonAir& air,
                                                       sc::physics::board_runtime::BoardRuntime& board,
                                                       const Transform& reckoning, ProcessedPhysicsInput& p,
                                                       SkeletonOwners& owners,
                                                       sc::animation::Slice<const NativeMatrix> globals,
                                                       const CollisionInput& collision,
                                                       sc::physics::rigid_body::RetailSimulationStep simulation,
                                                       bool fast_blend);
    ///82BDE600. target_com is PhysicsAir's actually integrated COM trajectory
    ///position (82D34A28 r5), not a predicted board position or skeleton root.
    ///The animation packet is the real82B985E8 publication, including its
    ///consumed dismount-revert request and retained frame count.
    rs::Result<Transform, std::string> update_known_air(
        skeleton_air::SkeletonAir& air, sc::physics::board_runtime::BoardRuntime& board, const Transform& reckoning,
        std::array<float, 4> target_com, const sc::animation::output::physics_packet::PhysicsPosePacket& packet,
        ProcessedPhysicsInput& p, SkeletonOwners& owners, sc::animation::Slice<const NativeMatrix> globals,
        const CollisionInput& collision, sc::physics::rigid_body::RetailSimulationStep simulation);

    // ---- offboard/skeleton_air.rs (group A) ----
    ///Old-root skate/COM -> new animation root -> held-board target ->
    ///GeneralUpdate -> shared Reckoning -> clear next trajectory12176.
    ///Return the actual processed0..48 target for the parent's input owner.
    /// Rust generic `F: FnOnce(ReckoningUpdate, &ProcessedPhysicsInput, f32) -> Result<(), String>`.
    rs::Result<Transform, std::string> update_biped_air(
        skeleton_air::SkeletonAir& air, sc::physics::board_runtime::BoardRuntime& board,
        offboard::skeleton_air::Input input, ProcessedPhysicsInput& p, SkeletonOwners& owners,
        sc::animation::Slice<const NativeMatrix> globals, const CollisionInput& collision,
        sc::physics::rigid_body::RetailSimulationStep simulation,
        std::function<Status(offboard::skeleton_ground::ReckoningUpdate, const ProcessedPhysicsInput&, float)>
            finish_reckoning);

    // ---- offboard/skeleton_ground.rs (group A) ----
    /// Rust generic `F: FnOnce(ReckoningUpdate) -> Result<(), String>`.
    rs::Result<Transform, std::string> update_biped_ground(
        skeleton_air::SkeletonAir& air, sc::physics::board_runtime::BoardRuntime& board,
        offboard::skeleton_ground::Input input, offboard::skeleton_ground::state::State& state,
        ProcessedPhysicsInput& p, SkeletonOwners& owners, sc::animation::Slice<const NativeMatrix> globals,
        const CollisionInput& collision, sc::physics::rigid_body::RetailSimulationStep simulation,
        std::function<Status(offboard::skeleton_ground::ReckoningUpdate)> finish_reckoning);
};

/// Private in Rust.
void publish_attribute_flags(const animation_input::AnimationInput& input, ProcessedPhysicsInput& p);

}  // namespace s3::game::physics::skeleton_input_runtime

#include "s3/game/physics/skeleton_input_general.hpp"
#include "s3/game/physics/skeleton_input_teleport.hpp"
