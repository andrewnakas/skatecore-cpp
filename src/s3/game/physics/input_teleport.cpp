// Port of skate-game/src/physics/input_teleport.rs
// (mounted as `physics::input_phase::teleport` via #[path] in input_phase.rs).
//! Ordinary local-player teleport82DB8998, operating on the live assemblies.
#include "s3/game/physics/input_teleport.hpp"

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/game/physics/animated_skeleton.hpp"
#include "s3/game/physics/animation_input.hpp"
#include "s3/game/physics/biped_ground/grab_runtime.hpp"
#include "s3/game/physics/footplant.hpp"
#include "s3/game/physics/ground_phase.hpp"
#include "s3/game/physics/ground_runtime.hpp"
#include "s3/game/physics/handplant.hpp"
#include "s3/game/physics/input_phase.hpp"
#include "s3/game/physics/player_input.hpp"
#include "s3/game/physics/riding_outputs.hpp"
#include "s3/game/physics/settings.hpp"
#include "s3/game/physics/skeleton_air.hpp"
#include "s3/game/physics/skeleton_output.hpp"
#include "s3/game/physics/wipeout.hpp"

namespace s3::game::physics::input_phase {

using sc::math::Basis3;
using sc::math::Vector3;
using skeleton_input_runtime::SkeletonOwners;

Status Callbacks::reset_player(sc::physics::board_runtime::BoardRuntime& board, ground_runtime::GroundRuntime& runtime,
                               sc::animation::output::NativeMatrix requested,
                               sc::player::input_phase::PlayerInputState& player,
                               sc::player::input_phase::PhysicalPlayerInput& physical,
                               sc::player::input_phase::ProcessedPhysicsInput& p) {
    S3G_CHECK(life.skeleton_controller.request_ground(collision_mode));
    const auto target = teleport::horizontal_spawn(requested);
    log::info(fmt::cat("player teleport spawn on_board=", (p.byte_1600 & 1u) != 0, " requested=(", requested[3][0],
                       ",", requested[3][1], ",", requested[3][2], ") applied=(", target[3][0], ",", target[3][1],
                       ",", target[3][2], ")"));
    player.flags_1296 = ((player.flags_1296 & 0xffef'ffffu) & 0x81ff'ffffu) | 0x6000'0000u;
    player.previous_spin_input_1360 = 0.0f;
    player.spin_same_direction_frames_1324 = 0;
    player.dismount_request_frames_1332 = 0;
    player.time_on_ground_1352 = 0.0f;
    player.signed_ground_time_1356 = 0.0f;
    player.queued_vector_1280 = {0, 0, 0, 0};
    player.hips_line_test_1488 = {};
    player.left_line_test_1536 = {};
    player.right_line_test_1584 = {};
    player_input::reset_outputs(physical);
    board.reset_physical(settings.authored, teleport::affine(target), p.flags_2468,
                         settings.step.simulation.gravity_acceleration);
    ground_phase::reset_board_state(ground, runtime, life, wiping_out);
    skeleton_air.reset_board();  // Board Reset82C0606C.
    footplant.full_reset();
    handplant.full_reset();
    riding.reset_for_teleport();
    {
        SkeletonOwners owners{animated, body, drives, ik, animation_input, output.correction, pose_errors};
        skeleton_input.reset_for_teleport(owners, collision_mode, feedback, output.wobble,
                                          life.skeleton_elapsed_16505);
    }
    // ProcessData below reads the reset collision manager, not the snapshot
    // taken before this callback entered ResetSystems.
    collision.contact_4070 = feedback.flags.compliant;
    collision.has_pose_error_4077 = feedback.flags.has_impulse;
    collision.partial_ragdoll = collision_mode.partial_ragdoll;
    collision.drive_weight_4028 = feedback.drive_weight;
    // ResetSystems82DB92D0 preserves Reckoning filter histories. Seed the
    // fresh deck heading and recompute its actual Ground transform.
    riding.reckoning.reset();
    riding.update_ground_reckoning(
        board,
        riding_outputs::RidingPoseInputs{
            Vector3::new_(animated.record.com_to_deck_world[0], animated.record.com_to_deck_world[1],
                          animated.record.com_to_deck_world[2]),
            animation_input.extra.physical_body_spin,
        },
        p.flags_2468, animation_input.fields.balance, false, p);
    wipeout.state.reset_systems();
    life.skeleton_controller.effective = 0;
    life.skeleton_controller.requested = 0;
    life.skeleton_controller.has_request = false;
    life.skeleton_controller.override_enabled = false;
    life.skeleton_controller.flag_18 = false;
    // 82DB93E8/93EC resets Player1840 through82D749D0, not ballistic work.
    offboard_grab.invalidate();
    player.manager_1856_counter_320 = 0;
    player.probe = {};

    const auto toolkit = runtime.prepare_toolkit(board, p);
    S3G_CHECK(process_skeleton(board, toolkit, packet, physical, p));
    {
        SkeletonOwners owners{animated, body, drives, ik, animation_input, output.correction, pose_errors};
        S3G_CHECK(skeleton_input.update_teleport(board, riding.reckoning_frames.system, p, owners, globals, collision,
                                                 settings.step.simulation));
    }
    wipeout.state.teleport();  // 82DB8DB4, separate from ResetSystems.
    teleported = true;
    return ok();
}

namespace teleport {

sc::physics::drive_frames::RetailAffineTransform affine(NativeMatrix m) {
    sc::physics::drive_frames::RetailAffineTransform out{};
    for (std::size_t i = 0; i < 3; ++i) out.basis.columns[i] = {m[i][0], m[i][1], m[i][2]};
    out.translation = Vector3::new_(m[3][0], m[3][1], m[3][2]);
    return out;
}

NativeMatrix horizontal_spawn(NativeMatrix requested) {
    NativeMatrix result = sc::physics::skeleton_animation_record::IDENTITY;
    const float x = requested[2][0];
    const float z = requested[2][2];
    const float length = rs::sqrt(x * x + z * z);
    if (length > rs::from_bits(0x3a83'126fu)) {
        result[2] = {x / length, 0.0f, z / length, 0.0f};
        result[0] = {result[2][2], 0.0f, -result[2][0], 0.0f};
    }
    result[3] = requested[3];
    result[3][1] += 0.1f;
    return result;
}

}  // namespace teleport

}  // namespace s3::game::physics::input_phase
