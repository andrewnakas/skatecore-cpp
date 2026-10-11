// Port of skate-game/src/physics/input_phase.rs.
//! Connect the animation packet to the one physical player and skeleton.
//! PlayerInput retains the original ordering; callbacks split host ownership.
//
// Submodule (#[path]): `teleport` = physics/input_teleport.rs (group C, namespace
// input_phase::teleport), which defines `Callbacks::reset_player` in input_teleport.cpp.
#pragma once
#include <string>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/output/native.hpp"
#include "s3/core/animation/slice.hpp"
#include "s3/core/input/controller.hpp"
#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/board_toolkit.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/game/common.hpp"
#include "s3/game/grind_world.hpp"  // group B: StaticProvider
#include "s3/game/physics/fwd.hpp"
#include "s3/game/physics/player_input/services.hpp"  // group C: PlayerInputCallbacks
#include "s3/game/physics/skeleton_input_runtime.hpp"

namespace s3::game::physics {
namespace animated_skeleton {
struct AnimatedSkeleton;
}
namespace animation_input {
struct AnimationInput;
}
namespace foot_ik {
struct FootIk;
}
namespace riding_outputs {
struct RidingOutputs;
}
namespace skeleton_air {
struct SkeletonAir;
}
namespace skeleton_output {
struct SkeletonOutput;
}
namespace footplant {
struct Footplant;  // group C
}
namespace handplant {
struct Handplant;  // group A
}
namespace ground_phase {
struct GroundLifecycle;  // group C
}
namespace ground_runtime {
struct GroundRuntime;  // group C
namespace state {
struct GroundState;  // group C (re-exported as ground_runtime::GroundState)
}
}  // namespace ground_runtime
namespace biped_ground::grab_runtime {
struct Owner;  // group A
}
namespace wipeout {
struct Wipeout;  // group A
}
namespace settings {
struct PhysicsSettings;  // coordinator
}
namespace grind_materials {
struct GrindMaterials;  // group B
}
}  // namespace s3::game::physics

namespace s3::game::physics::input_phase {

using skeleton_input_runtime::CollisionInput;

/// pub(super)
rs::Result<bool, std::string> advance(GamePhysics& physics, SkaterRuntime& skater,
                                      const sc::player::input_phase::AnimationInputPacket& packet,
                                      sc::input::controller::ActionMap& actions, bool input_available);

/// Name the first non-finite reckoning input, once per run. (Private in Rust.)
void report_non_finite_reckoning(const GamePhysics& physics, const SkaterRuntime& skater);

/// pub(super)
Status update_ground(const GamePhysics& physics, SkaterRuntime& skater);

/// pub(super)
CollisionInput collision(const SkaterRuntime& skater);

/// Private in Rust (`struct Callbacks<'a, 'p>`); every `&'a mut` borrow is a reference.
/// `impl PlayerInputCallbacks for Callbacks`; `reset_player` lives in input_teleport.rs.
struct Callbacks : player_input::services::PlayerInputCallbacks {
    skeleton_input_runtime::SkeletonInputRuntime& skeleton_input;
    animated_skeleton::AnimatedSkeleton& animated;
    skeleton_air::SkeletonAir& skeleton_air;
    footplant::Footplant& footplant;
    handplant::Handplant& handplant;
    sc::physics::skeleton_body::SkeletonBody& body;
    sc::physics::skeleton_body::SkeletonDrives& drives;
    foot_ik::FootIk& ik;
    animation_input::AnimationInput& animation_input;
    skeleton_output::SkeletonOutput& output;
    sc::physics::skeleton_body::SkeletonPoseErrors& pose_errors;
    sc::physics::skeleton_body::SkeletonCollisionMode& collision_mode;
    sc::physics::skeleton_body::SkeletonCollisionFeedback& feedback;
    CollisionInput collision;
    ground_runtime::state::GroundState& ground;
    ground_phase::GroundLifecycle& life;
    biped_ground::grab_runtime::Owner& offboard_grab;
    wipeout::Wipeout& wipeout;
    riding_outputs::RidingOutputs& riding;
    bool& wiping_out;
    settings::PhysicsSettings& settings;
    const grind_materials::GrindMaterials& grind_materials;
    bool air_targeting_grind;
    sc::animation::Slice<const sc::animation::output::NativeMatrix> globals;
    sc::animation::Slice<const sc::animation::output::attributes::AnimationAttribute> attributes;
    sc::input::controller::ActionMap& actions;
    const sc::player::input_phase::AnimationInputPacket& packet;
    bool teleported;

    Callbacks(skeleton_input_runtime::SkeletonInputRuntime& skeleton_input_,
              animated_skeleton::AnimatedSkeleton& animated_, skeleton_air::SkeletonAir& skeleton_air_,
              footplant::Footplant& footplant_, handplant::Handplant& handplant_,
              sc::physics::skeleton_body::SkeletonBody& body_, sc::physics::skeleton_body::SkeletonDrives& drives_,
              foot_ik::FootIk& ik_, animation_input::AnimationInput& animation_input_,
              skeleton_output::SkeletonOutput& output_, sc::physics::skeleton_body::SkeletonPoseErrors& pose_errors_,
              sc::physics::skeleton_body::SkeletonCollisionMode& collision_mode_,
              sc::physics::skeleton_body::SkeletonCollisionFeedback& feedback_, CollisionInput collision_,
              ground_runtime::state::GroundState& ground_, ground_phase::GroundLifecycle& life_,
              biped_ground::grab_runtime::Owner& offboard_grab_, wipeout::Wipeout& wipeout_,
              riding_outputs::RidingOutputs& riding_, bool& wiping_out_, settings::PhysicsSettings& settings_,
              const grind_materials::GrindMaterials& grind_materials_, bool air_targeting_grind_,
              sc::animation::Slice<const sc::animation::output::NativeMatrix> globals_,
              sc::animation::Slice<const sc::animation::output::attributes::AnimationAttribute> attributes_,
              sc::input::controller::ActionMap& actions_,
              const sc::player::input_phase::AnimationInputPacket& packet_, bool teleported_)
        : skeleton_input(skeleton_input_), animated(animated_), skeleton_air(skeleton_air_), footplant(footplant_),
          handplant(handplant_), body(body_), drives(drives_), ik(ik_), animation_input(animation_input_),
          output(output_), pose_errors(pose_errors_), collision_mode(collision_mode_), feedback(feedback_),
          collision(collision_), ground(ground_), life(life_), offboard_grab(offboard_grab_), wipeout(wipeout_),
          riding(riding_), wiping_out(wiping_out_), settings(settings_), grind_materials(grind_materials_),
          air_targeting_grind(air_targeting_grind_), globals(globals_), attributes(attributes_), actions(actions_),
          packet(packet_), teleported(teleported_) {}

    // ---- impl PlayerInputCallbacks for Callbacks (input_phase.cpp) ----
    rs::Result<player_input::grind::Pending, std::string> prepare_grind(
        sc::physics::board_runtime::BoardRuntime& board, player_input::grind::GrindInputState& grind,
        const sc::player::input_phase::ProcessedPhysicsInput& processed,
        const sc::physics::board_world::BoardWorld& world, const grind_world::StaticProvider& provider,
        std::int32_t air_counter) override;
    Status process_skeleton(sc::physics::board_runtime::BoardRuntime& board,
                            const sc::physics::board_toolkit::BoardToolkit& toolkit,
                            const sc::player::input_phase::AnimationInputPacket& packet,
                            sc::player::input_phase::PhysicalPlayerInput& physical,
                            sc::player::input_phase::ProcessedPhysicsInput& processed) override;
    Status teleport(sc::physics::board_runtime::BoardRuntime& board, ground_runtime::GroundRuntime& runtime,
                    sc::animation::output::NativeMatrix target, sc::player::input_phase::PlayerInputState& player,
                    sc::player::input_phase::PhysicalPlayerInput& physical,
                    sc::player::input_phase::ProcessedPhysicsInput& processed) override;

    // ---- input_teleport.rs (group C, input_teleport.cpp) ----
    /// pub(super)
    Status reset_player(sc::physics::board_runtime::BoardRuntime& board, ground_runtime::GroundRuntime& runtime,
                        sc::animation::output::NativeMatrix requested, sc::player::input_phase::PlayerInputState& player,
                        sc::player::input_phase::PhysicalPlayerInput& physical,
                        sc::player::input_phase::ProcessedPhysicsInput& p);
};

}  // namespace s3::game::physics::input_phase
