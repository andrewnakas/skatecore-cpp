// Port of skate-game/src/physics/biped_ground.rs
#include "s3/game/physics/biped_ground.hpp"

#include "s3/core/player/offboard/ground_input.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/input_phase.hpp"
#include "s3/game/physics/offboard/settings.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/skeleton_input_runtime.hpp"

namespace s3::game::physics::biped_ground {

namespace controller = ::s3::player::offboard::controller;

///Only the shared82D8E3E0 callee, not Ground movement/lifecycle.
///Ground82BDE2CC and Air82BDDFFC both supply enable-body-spin r7=1.
///Each caller supplies its own native up/forward/blend and phase ordering.
::s3::player::offboard::ground_reckoning::Output Owner::finish_reckoning(
    ::s3::game::physics::offboard::skeleton_ground::ReckoningUpdate update, riding_outputs::RidingOutputs& riding,
    air_reckoning::AirReckoning& air, const ::s3::player::input_phase::ProcessedPhysicsInput& processed,
    float physical_body_spin) const {
    return skeleton_state.finish_reckoning(update, riding, air, processed, physical_body_spin);
}

std::tuple<::s3::point_graph::PointGraph<8>, ::s3::player::offboard::air_launch::Settings> Owner::air_settings()
    const {
    return {turn_vs_speed(), air_launch};
}

::s3::point_graph::PointGraph<8> Owner::turn_vs_speed() const {
    return controller.settings.movement_velocity.turn_vs_speed;
}

rs::Result<Owner, std::string> Owner::load(const sd::Collections& data, const sd::AnimationMetadata& metadata) {
    S3G_TRY(auto settings, ::s3::game::physics::offboard::settings::Settings::load(data, metadata));
    const auto grab_settings = settings.board;
    auto [controller_settings, metrics, movement_vs_stick_angle, turn_vs_stick_angle, air_launch] =
        std::move(settings).into_controller_parts();
    S3G_TRY(auto skeleton_state, ::s3::game::physics::offboard::skeleton_ground::State::load(data));
    S3G_TRY(auto geometry, ::s3::game::physics::offboard::ground_geometry::State::load(data));
    S3G_TRY(auto collision_settings, lifecycle::load_collision_settings(data));
    return Owner{
        controller::Controller::new_(std::move(controller_settings), metrics),
        std::nullopt,
        ground_entry::State{},
        contact_toolkit::ContactPrefix::reset(),
        std::nullopt,
        std::move(skeleton_state),
        std::move(geometry),
        movement_vs_stick_angle,
        turn_vs_stick_angle,
        air_launch,
        collision_settings,
        grab_settings,
    };
}

void Owner::reset(contact_toolkit::Owner& toolkit) {
    //82D30BD0 is a Ground-state reset, not Biped82D7B1C0.
    //Keep controller cadence, correction vectors and shared candidate alive.
    result.reset();
    ground.flags_144_to_150 = {false, false, false, false, false, false, false};
    ground.counter_152 = 0;
    ground.counter_156 = 0;
    ground.elapsed_160 = 0.0f;
    toolkit.reset_history();
    ground = ground_entry::State{};
    contact = contact_toolkit::ContactPrefix::reset();
    geometry_adjustment.reset();
}

controller::GroundResult Owner::run(controller::GroundJob job) {
    const auto out = controller.step_ground(job);
    result = out;
    return out;
}

void Owner::enter(ground_entry::Input input, std::uint32_t current_state,
                  std::array<std::array<float, 4>, 4> previous_frame, contact_toolkit::Owner& toolkit) {
    reset(toolkit);
    geometry.reset();
    const auto placement = ground.enter(input);
    controller::PlacementInput place{};
    place.frame = placement.frame;
    place.velocity = placement.planar_velocity;
    place.body_position = placement.body_position;
    place.current_state = current_state;
    place.previous_state = input.previous_state_2504;
    place.previous_frame = previous_frame;
    controller.place(place);
}

Status enter(GamePhysics& /*physics*/, SkaterRuntime& skater) {
    const auto& p = skater.player_input.processed;
    //82D30848 calls GetEffectiveRoot82BE3650 BEFORE Ground placement.
    const auto frame = entry::effective_root(skater.animated_skeleton.roots.animation_to_world, p.flags_2476);
    std::array<std::array<float, 4>, 4> previous_frame{};
    for (std::size_t i = 0; i < 4; ++i) previous_frame[i] = rs::from_bits(p.effective_anim_transform_192[i]);
    ground_entry::Input input{};
    input.animation_frame = frame;
    input.processed_velocity_608 = rs::from_bits(p.vectors_544_560_592_608[3]);
    input.processed_flags_2484 = p.flags_2484;
    input.processed_flags_2476 = p.flags_2476;
    input.requested_angle_2936 = skater.animation_input.extra.biped_start_angle;
    input.requested_duration_2896 = skater.animation_input.fields.animation_time;
    input.previous_state_2504 = p.state_2504;
    input.previous_frame_up_208 = previous_frame[1];
    //82BDE398..3E0: COM frame15824 + translation48 = Skeleton15872.
    input.body_position_15872 = skater.animated_skeleton.board_frames.com_frame[3];
    skater.biped_ground.enter(input, p.state_2508, previous_frame, skater.offboard_contact);
    //82D30B44: actual state56 ConsiderReset, after placement and before mode4.
    services::enter_feet(skater);
    //82D30B54 requests the shared skeleton collision controller's mode4.
    S3G_CHECK(skater.ground_lifecycle.skeleton_controller.request(4, skater.skeleton_collision));
    //82D30B5C..B64: state52 grab manager, NOT SkateboardController60.
    skater.offboard_grab.enter_reset();
    return ok();
}

void exit(SkaterRuntime& skater) {
    //82D30CD8 refreshes, then30CE4..CF8 clears readiness/history before
    //releasing Ground geometry. Candidate and Biped cadence remain alive.
    skater.offboard_contact.reset_history();
    //82D30CC0 releases the ground query. It does NOT reset shared Biped
    //motion/cadence/correction; BipedAir and subsequent placement consume them.
    skater.biped_ground.geometry.reset();
}

rs::Result<contact_toolkit::Input, std::string> update(GamePhysics& physics, SkaterRuntime& skater,
                                                        const PlayerControls& /*controls*/,
                                                        ground_job::ContactSnapshot contact) {
    const auto p = skater.player_input.processed;
    const auto fields = skater.animation_input.fields;
    const auto extra = skater.animation_input.extra;
    float animation_motion_sq = -0.0f;
    for (std::size_t i = 0; i < 3; ++i) {
        const float value = fields.animation_translation[i];
        animation_motion_sq += value * value;
    }
    if ((p.flags_2488 & 0x08000000u) != 0 && skater.biped_ground.controller.state.motion.target_scale_784 < 0.0f &&
        animation_motion_sq < rs::from_bits(0x3a83126fu) && !(fields.animation_time > 0.0f)) {
        // Temporary first-producer trace. Native Ground movement82D7F4BC enters
        // the directed branch and82D7F624 divides target distance by AnimTime;
        // stock graph publication must make this packet valid before that call.
        log::error(fmt::cat("OFFBOARD_DIRECTED_INPUT_INVALID state=", p.state_2508, "/", p.category_2512,
                            " previous=", p.state_2504, " filtered=", skater.player_input.physical.filtered_state_0,
                            " flags=", p.flags_2468, "/", p.flags_2472, "/", p.flags_2476, "/", p.flags_2480, "/",
                            p.flags_2488, " offboard_edge_distance=",
                            skater.player_input.physical.off_board.distance_116,
                            " offboard_obstacle_distance=", skater.player_input.physical.off_board.scalar_112,
                            " anim_time=", fields.animation_time, " target_scale=",
                            skater.biped_ground.controller.state.motion.target_scale_784));
    }
    const auto frame = skater.biped_ground.ground.frame_80;
    ::s3::player::offboard::ground_input::GroundInput input{};
    input.processed_flags_2472 = p.flags_2472;
    input.processed_direct_2684 = extra.offboard_magnitude;
    input.processed_direct_2680 = extra.offboard_turn;
    input.processed_stick_2692 = extra.biped_world_x;
    input.processed_stick_2688 = extra.biped_world_z;
    input.processed_scale_2912 = fields.magnitude_scale;
    input.processed_scale_2908 = fields.turn_scale;
    input.frame_forward_112 = {frame[2][0], frame[2][1], frame[2][2]};
    const auto prepared = ::s3::player::offboard::ground_input::calculate(
        input, skater.biped_ground.movement_vs_stick_angle, skater.biped_ground.turn_vs_stick_angle);
    const auto third_line = p.line_tests_960_1008_1056[2];
    auto& owner = skater.biped_ground;
    ground_job::Input job_input{};
    job_input.contact = contact;
    job_input.frame = frame;
    job_input.previous_state = p.state_2504;
    if (third_line.valid != 0) job_input.third_line_position = rs::from_bits(third_line.position);
    job_input.frames_since_teleport = p.frames_since_teleport_2584;
    job_input.processed_position = rs::from_bits(p.vectors_544_560_592_608[2]);
    job_input.processed_velocity = rs::from_bits(p.vectors_544_560_592_608[3]);
    job_input.controls = prepared;
    job_input.collision_displacements = skater.collision_extra_errors;
    job_input.animation_motion = fields.animation_translation;
    job_input.animation_velocity = skater.animated_skeleton.motion.velocity_world;
    job_input.requested_duration = fields.animation_time;
    job_input.requested_phase = fields.cadence_end_percent;
    job_input.override_duration = fields.animation_physics_blend_seconds;
    job_input.flags_2472 = p.flags_2472;
    job_input.flags_2476 = p.flags_2476;
    job_input.flags_2480 = p.flags_2480;
    job_input.flags_2484 = p.flags_2484;
    job_input.flags_2488 = p.flags_2488;
    S3G_TRY(const auto job,
            (ground_job::prepare<std::string>(
                owner.contact, owner.ground.distance_164, job_input,
                [&](::s3::player::offboard::ground_query::ConsumeInput consume)
                    -> rs::Result<::s3::player::offboard::ground_query::GroundAdjustment, std::string> {
                    return owner.geometry.consume(consume);
                })));
    owner.geometry_adjustment = job.geometry;
    const auto result = owner.run(job.job);
    bool nonfinite = false;
    for (const auto& v : result.physical_frame)
        for (float x : v) nonfinite |= !rs::is_finite(x);
    for (const auto& v : result.animation_frame)
        for (float x : v) nonfinite |= !rs::is_finite(x);
    for (const auto& v : result.surface_frame)
        for (float x : v) nonfinite |= !rs::is_finite(x);
    for (float x : result.velocity) nonfinite |= !rs::is_finite(x);
    for (float x : result.position) nonfinite |= !rs::is_finite(x);
    if (nonfinite) {
        // Temporary first-divergence trace; do not repair or suppress the value.
        log::error("OFFBOARD_GROUND_RESULT_NONFINITE");
    }
    //82D31CE8..D04 is Ground FeetIK, not Air's timer-reset wrapper.
    services::update_feet(skater);
    //82D31D14 calls the shared board steering owner with zero input before
    //possession. Offboard Ground must still advance its tilt and timers.
    skater.ground.steering.update(0.0f, skater.air_settings.steering_blend, p.flags_2468, p.flags_2472);
    //82D31D1C..DB0 operates the EXISTING carried-board controller.
    ground_board::update_possession(physics, skater);
    //82D32338 precedes contact correction and angular unwind. Submit through
    //the parent's persistent selector; Ground never selects Air speculatively.
    //Current authored scene has no grind edges. Typed absence is accepted by
    //every native non-mode1 launch; no fabricated departure vectors.
    S3G_TRY(const auto launch_input, services::launch_input(skater, std::nullopt));
    S3G_TRY(const auto packet, skater.biped_ground.prepare_air(launch_input));
    if (packet) {
        S3G_CHECK(services::submit_air(physics, skater, *packet));
    }
    auto& owner2 = skater.biped_ground;
    owner2.ground.flags_144_to_150[0] = false;
    // Sync82D31E64..82D32070: physical contact correction and the Enter
    // angular unwind must precede BOTH Skeleton and the next query submission.
    const auto animation_frame = ground_job::sync_frames(owner2.ground, owner2.contact, result);
    const auto collision = input_phase::collision(skater);
    //82D32084 passes state1056 (GroundResult208), not processed752.
    const auto centre_of_mass = result.position;
    const float body_spin = skater.animation_input.extra.physical_body_spin;
    const auto simulation = physics.settings.step.simulation;
    skeleton_input_runtime::SkeletonOwners owners{
        skater.animated_skeleton, skater.skeleton,   skater.skeleton_drives,         skater.foot_ik,
        skater.animation_input,   skater.skeleton_output.correction, skater.pose_errors,
    };
    std::optional<::s3::game::physics::offboard::skeleton_ground::ReckoningUpdate> reckoning_update;
    ::s3::game::physics::offboard::skeleton_ground::Input skeleton_input{&animation_frame, centre_of_mass};
    S3G_CHECK(skater.skeleton_input.update_biped_ground(
        skater.skeleton_air, physics.board, skeleton_input, skater.biped_ground.skeleton_state,
        skater.player_input.processed, owners, skater.animation.packet.hierarchy, collision, simulation,
        [&](::s3::game::physics::offboard::skeleton_ground::ReckoningUpdate update) -> Status {
            reckoning_update = update;
            return ok();
        }));
    if (reckoning_update) {
        (void)skater.biped_ground.skeleton_state.finish_reckoning(*reckoning_update, physics.riding,
                                                                  skater.air_reckoning,
                                                                  skater.player_input.processed, body_spin);
    }
    //82D32088..32108: real grab-spline host executes grab::sync here.
    services::sync_grab(skater);
    //82D32130..164: surface-frame axes, NOT physical-frame axes.
    contact_toolkit::Input out{};
    out.position = skater.biped_ground.ground.frame_80[3];
    out.forward = result.surface_frame[2];
    out.up = result.surface_frame[1];
    out.right = result.surface_frame[0];
    out.velocity = result.velocity;
    out.animation_up = animation_frame[1];
    out.animation_right = animation_frame[0];
    return out;
}

/// Call immediately AFTER submitting update's toolkit input through the shared
/// owner. The seven real line results become visible at the NEXT PreUpdate.
Status submit_geometry(const GamePhysics& physics, SkaterRuntime& skater) {
    const auto& p = skater.player_input.processed;
    auto& owner = skater.biped_ground;
    S3G_TRY_OPT(const auto result, owner.result, "Ground geometry submission requires completed motion");
    return owner.geometry.submit(physics.world, owner.ground.frame_80, result.velocity,
                                 ::s3::player::offboard::ground_query::QueryContext{
                                     p.actor_query_2948, static_cast<std::int32_t>(p.actor_query_2952)},
                                 p.flags_2488);
}

}  // namespace s3::game::physics::biped_ground
