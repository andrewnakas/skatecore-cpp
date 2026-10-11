// Port of skate-game/src/physics/input_phase.rs.
//! Connect the animation packet to the one physical player and skeleton.
//! PlayerInput retains the original ordering; callbacks split host ownership.
#include "s3/game/physics/input_phase.hpp"

#include <atomic>

#include "s3/game/physics.hpp"
#include "s3/game/physics/grind_host.hpp"
#include "s3/game/physics/offboard/board_manager/runtime.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/solve.hpp"

namespace s3::game::physics::input_phase {

using sc::player::input_phase::AnimationInputPacket;
using skeleton_input_runtime::SkeletonOwners;
using skeleton_input_runtime::SkeletonPoseInput;

namespace {
std::string debug_v4(const std::array<float, 4>& v) {
    return "[" + fmt::debug_f32(v[0]) + ", " + fmt::debug_f32(v[1]) + ", " + fmt::debug_f32(v[2]) + ", " +
           fmt::debug_f32(v[3]) + "]";
}
std::string debug_m4(const std::array<std::array<float, 4>, 4>& m) {
    return "[" + debug_v4(m[0]) + ", " + debug_v4(m[1]) + ", " + debug_v4(m[2]) + ", " + debug_v4(m[3]) + "]";
}
}  // namespace

rs::Result<bool, std::string> advance(GamePhysics& physics, SkaterRuntime& skater, const AnimationInputPacket& packet,
                                      sc::input::controller::ActionMap& actions, bool input_available) {
    // 82DB4094 completes the preceding off-board batch BEFORE input reset.
    skater.offboard_contact.begin_input();
    // 82DB409C clears the actual shared manager latch;82DB4348 publishes its IK offset.
    skater.landing_deck.manager.can_land_256 = false;
    skater.player_input.player.manager_1852_vector_176 = rs::to_bits(skater.landing_deck.manager.ik_offset_176);
    player_input::InputHostFrame host{};
    // Native actor queries are host identity, not a physical parameter.
    host.actor_query_56 = 0;
    host.actor_query_44 = 0;
    host.input_available = input_available;
    // Gameplay action **65**, left stick Y -- not 71, the right trigger.
    //
    // This is the "press up to fly out" input. Retail's store to Processed `+2636`
    // (`sub_82DB4048`, the clamp at `0x82DB4330`) is fed by `bl 0x825903C8`, and
    // `82590358`/`825903C8` are the stock cInputMap pair 64 and 65 -- so the transition input
    // is 65. `GameplayActions::value` indexes `values[action - 64]`, where `values[1]` is
    // `value(18) - value(19)` = left stick up minus down, and `values[7]` is the right
    // trigger.
    //
    // It reaches `+2636`, then the air trajectory selector as `directional_input`, where it
    // both tilts the launch (`launch.rs` `lean = clamp(input - 0.25, -1, 1)`) and casts the
    // fly-out vote (`scoring.rs`, `input >= 0.5` scores surfaces past the lip, below it
    // scores the transition face you came up). Reading the trigger instead left that vote
    // permanently at 0, so every quarter pipe launched the skater out over the coping instead
    // of arcing back into the ramp. Both the clamp to [-1, 1] and the `- 0.25` lean only make
    // sense for a bipolar stick axis, never for a 0..1 trigger.
    host.transition_action = actions.value(65);
    if (skater.player_input.physical.state.flag_61 != 0) {
        const auto& out = skater.player_input.physical.teleport_output;
        if (!out) return rs::make_err(std::string("Teleport State61 requires its published reset transform"));
        for (std::size_t i = 0; i < 4; ++i) host.published_board_transform[i] = rs::from_bits(out->transform[i]);
    } else {
        host.published_board_transform = solve::deck_frame(physics.board);
    }
    // 82D8ABD8 reads the state selector's retained signed counter+40,
    // before this tick's CalcSuggestedState advances it at82D8AF78..94.
    host.air_counter_40 = skater.player_state.selector.post_grind_jump_counter;
    // TU3 82DB8CE4..8D1C stops possession before moving either assembly.
    const bool resetting = skater.player_input.physical.state.flag_61 != 0 ||
                           (skater.player_input.player.flags_1296 & (1u << 19)) != 0 ||
                           skater.player_input.pending_teleport().has_value();
    if (resetting) {
        skater.offboard_air_selector.reset();
        offboard::board_manager::runtime::reset_for_teleport(physics, skater);
    }
    const CollisionInput collision_ = collision(skater);
    Callbacks callbacks(skater.skeleton_input, skater.animated_skeleton, skater.skeleton_air, skater.footplant,
                        skater.handplant, skater.skeleton, skater.skeleton_drives, skater.foot_ik,
                        skater.animation_input, skater.skeleton_output, skater.pose_errors, skater.skeleton_collision,
                        skater.collision_feedback, collision_, skater.ground, skater.ground_lifecycle,
                        skater.offboard_grab, skater.wipeout, physics.riding, physics.board_wiping_out,
                        physics.settings, physics.grind_materials,
                        skater.trajectory.selector.grind_locked_to_middle(), skater.animation.packet.hierarchy,
                        skater.animation.attributes.entries(), actions, packet, false);
    const auto ground_frame = callbacks.riding.reckoning_frames.ground;
    S3G_TRY(const auto continuation,
            skater.player_input.process_stage(physics.board, skater.ground_runtime, ground_frame, packet, host,
                                              callbacks, player_input::InputStage::ThroughTeleport{}, physics.world,
                                              *physics.grind_world));
    if (continuation.has_value()) {
        S3G_CHECK(skater.player_input.process_stage(physics.board, skater.ground_runtime, ground_frame, packet, host,
                                                    callbacks, player_input::InputStage::AfterTeleport{*continuation},
                                                    physics.world, *physics.grind_world));
    }
    const bool teleported = callbacks.teleported;
    if (teleported) {
        // TU3 82DB8D6C..8DA4 repeats the controller reset after Skeleton.
        offboard::board_manager::runtime::reset_for_teleport(physics, skater);
    }
    physics.processed_flags_2468 = skater.player_input.processed.flags_2468;
    return teleported;
}

/// Name the first non-finite reckoning input, once per run.
///
/// The landing abort ("Non-finite linear_velocity before shared solve") is reported from the
/// solver, long after the fact: by then `animation_to_world`, `com_frame` and the lifted-COM
/// target velocity are all NaN and the origin is gone. Two of these are *persistent* -- `heading`
/// falls back to itself, and `body_flip` is composed into `system` every frame -- so one
/// degenerate tick poisons the rest of the session. The Gram-Schmidt guards keep this from
/// reaching the solver; this says which input went bad, so the cause can be fixed rather than
/// contained.
void report_non_finite_reckoning(const GamePhysics& physics, const SkaterRuntime& skater) {
    static std::atomic<bool> REPORTED{false};
    if (REPORTED.load(std::memory_order_relaxed)) return;
    const auto& frames = physics.riding.reckoning_frames;
    auto rows = [](const std::array<std::array<float, 4>, 4>& m) {
        for (const auto& r : m)
            for (float v : r)
                if (!std::isfinite(v)) return false;
        return true;
    };
    auto finite4 = [](const std::array<float, 4>& v) {
        for (float x : v)
            if (!std::isfinite(x)) return false;
        return true;
    };
    const std::array<std::pair<const char*, bool>, 4> bad{{
        {"reckoning.system", rows(frames.system)},
        {"reckoning.body_flip", rows(frames.body_flip)},
        {"reckoning.heading", finite4(frames.heading)},
        {"animated_skeleton.board_pose", rows(skater.animated_skeleton.record.pose[0])},
    }};
    for (const auto& [name, ok] : bad) {
        if (!ok) {
            REPORTED.store(true, std::memory_order_relaxed);
            log::error(fmt::cat("first non-finite reckoning input: ", name, " (system=", debug_m4(frames.system),
                                " body_flip=", debug_m4(frames.body_flip), " heading=", debug_v4(frames.heading),
                                ")"));
            break;
        }
    }
}

Status update_ground(const GamePhysics& physics, SkaterRuntime& skater) {
    report_non_finite_reckoning(physics, skater);
    const CollisionInput collision_ = collision(skater);
    SkeletonOwners owners{skater.animated_skeleton,   skater.skeleton,  skater.skeleton_drives,
                          skater.foot_ik,             skater.animation_input,
                          skater.skeleton_output.correction, skater.pose_errors};
    S3G_TRY(const auto target,
            skater.skeleton_input.update_ground(physics.board, physics.riding.reckoning_frames.system,
                                                skater.player_input.processed, owners,
                                                skater.animation.packet.hierarchy, collision_,
                                                physics.settings.step.simulation));
    // Ground82D38000 completes GeneralUpdate;82D38008 then retains the actual
    // board/animation error used when Air subsequently blends its board target.
    skater.skeleton_air.capture_physics_error(physics.board, target);
    return ok();
}

CollisionInput collision(const SkaterRuntime& skater) {
    return CollisionInput{
        skater.collision_feedback.flags.compliant,
        skater.collision_feedback.flags.has_impulse,
        skater.collision_pose_error,
        skater.skeleton_collision.partial_ragdoll,
        skater.collision_feedback.drive_weight,
    };
}

rs::Result<player_input::grind::Pending, std::string> Callbacks::prepare_grind(
    sc::physics::board_runtime::BoardRuntime& board, player_input::grind::GrindInputState& grind,
    const sc::player::input_phase::ProcessedPhysicsInput& processed, const sc::physics::board_world::BoardWorld& world,
    const grind_world::StaticProvider& provider, std::int32_t air_counter) {
    const auto& extra = animation_input.extra;
    player_input::grind::PreContext context{};
    context.board = solve::deck_frame(board);
    context.air_counter = air_counter;
    context.tip_state = processed.state_2504;
    context.air_targeting_grind_9653 = air_targeting_grind;
    context.balance_2720 = animation_input.fields.balance;
    context.translation_2796 = extra.grind_translation;
    context.stability_nudge_2800 = extra.grind_stability_nudge;
    context.up_down_2804 = extra.grind_up_down;
    context.grab_min_height_2808 = extra.grind_grab_min_height;
    grind_host::LiveHost host(board, settings, grind_materials);
    return grind.pre_update(processed, provider, world, context, host);
}

Status Callbacks::process_skeleton(sc::physics::board_runtime::BoardRuntime& board,
                                   const sc::physics::board_toolkit::BoardToolkit& toolkit,
                                   const AnimationInputPacket& packet_,
                                   sc::player::input_phase::PhysicalPlayerInput& physical,
                                   sc::player::input_phase::ProcessedPhysicsInput& processed) {
    S3G_CHECK(animation_input.select_physics_mode(processed.state_variant_index_2528));
    SkeletonOwners owners{animated, body, drives, ik, animation_input, output.correction, pose_errors};
    return skeleton_input.process_data(board, toolkit, packet_, physical, processed, owners,
                                       SkeletonPoseInput{globals, attributes, actions}, collision);
}

Status Callbacks::teleport(sc::physics::board_runtime::BoardRuntime& board, ground_runtime::GroundRuntime& runtime,
                           sc::animation::output::NativeMatrix target,
                           sc::player::input_phase::PlayerInputState& player,
                           sc::player::input_phase::PhysicalPlayerInput& physical,
                           sc::player::input_phase::ProcessedPhysicsInput& processed) {
    return reset_player(board, runtime, target, player, physical, processed);
}

}  // namespace s3::game::physics::input_phase
