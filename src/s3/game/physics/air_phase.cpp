// Port of skate-game/src/physics/air_phase.rs.
//! PhysicsAir200 production lifecycle82D34388/82D346C0/82D346A0.
//! Uses the live player, board, skeleton and selector; no alternate air solver.
#include "s3/game/physics/air_phase.hpp"

#include "s3/game/physics.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::air_phase {

using ::s3::air::state::AirMath;
using ::s3::air::state::AirTrajectory;
using ::s3::air::state::angle_between_vectors;
using ::s3::air::state::calculate_velocity_from_jump;
using ::s3::air::state::integrate_trajectory_fixed_step;
using ::s3::air::state::PhysicsAirState;
using ::s3::air::state::wrap_signed_angle;

Status enter(GamePhysics& physics, SkaterRuntime& skater) {
    const auto frame = input::frame(skater);
    auto& life = skater.ground_lifecycle;
    life.skeleton_controller.flag_18 = true;
    S3G_CHECK(life.skeleton_controller.request(7, skater.skeleton_collision));
    skater.foot_ik.enable_feet(true);
    S3G_CHECK(life.skeleton_controller.request(6, skater.skeleton_collision));
    physics.board.hook_mut().drive.enable_angular_only(life.board_animated_290);
    skater.footplant.enabled = false;
    skater.footplant.reset();
    auto& state = skater.air_state;
    state.selector_latch_174 = false;
    state.reached_apex = false;
    state.trajectory_query_countdown = 0;
    state.landing_normal = {0.0f, 1.0f, 0.0f, 0.0f};
    state.time_in_state = 0.0f;
    state.max_y = physics.board.part_transforms()[6].translation.y;
    state.start_y = frame.ground_position_y_500;
    bool use_com = false;
    bool heading = false;
    if (frame.previous_physics_category_2516 == 400 || frame.previous_physics_state_2504 == 701) {
        use_com = (frame.flags_2468 & 0x4000u) == 0;
        heading = false;
    } else {
        const auto prev = frame.previous_physics_state_2504;
        const bool selected = !(prev == 100 || prev == 103 || prev == 201);
        use_com = selected;
        heading = selected;
    }
    state.use_centre_of_mass_velocity = use_com;
    if (use_com) {
        auto velocity = frame.trajectory_velocity_608;
        if (frame.previous_physics_state_2504 == 500) {
            AirMath math;
            velocity[1] = math.minimum_vminfp(velocity[1] * rs::from_bits(0x3f266666u), 3.0f);
        }
        if (frame.frames_since_jump_correction_2576 < 12) {
            AirMath math;
            velocity = calculate_velocity_from_jump(frame, frame.frames_since_jump_correction_2576, math);
        }
        state.centre_of_mass_trajectory = AirTrajectory{
            frame.trajectory_position_592,
            velocity,
            COM_ACCELERATION,
            -1.0f,
        };
    }
    if (heading) {
        skater.animated_skeleton.roots.initialize_heading = true;
    }
    return ok();
}

void exit(SkaterRuntime& skater) {
    skater.air_state.use_centre_of_mass_velocity = false;
    skater.air_reckoning.state.reset_spin();
}

Status advance(GamePhysics& physics, SkaterRuntime& skater) {
    const auto frame = input::frame(skater);
    S3G_CHECK(skater.ground_lifecycle.skeleton_controller.update_air(
        skater.player_input.processed.flags_2472, frame.current_velocity_400[1], skater.skeleton_collision));
    {
        auto& state = skater.air_state;
        state.trajectory_query_countdown =
            static_cast<std::int32_t>(static_cast<std::uint32_t>(state.trajectory_query_countdown) - 1u);
        if (state.trajectory_query_countdown <= 0 && (frame.flags_2468 & 0x8000u) == 0 &&
            frame.previous_physics_state_2504 != 0 && !skater.trajectory.selector.pending()) {
            S3G_TRY(auto info, input::launch_info(physics, skater));
            if (frame.frames_since_jump_correction_2576 < 12) {
                AirMath math;
                info.start_velocity =
                    calculate_velocity_from_jump(frame, frame.frames_since_jump_correction_2576, math);
            } else if (skater.air_state.use_centre_of_mass_velocity) {
                //COM helper82D34570 repeats Fill, then offsets start/board and
                //narrows the cone. Caller clears player_jumped again before Launch.
                const auto velocity = skater.air_state.centre_of_mass_trajectory.velocity;
                for (std::size_t i = 0; i < 4; ++i) {
                    info.start_velocity[i] = std::fma(COM_ACCELERATION[i], rs::from_bits(0x3c888889u), velocity[i]);
                }
                info.start_position_override = info.animation_com_position;
                info.start_position_override[1] += rs::from_bits(0xbf266666u);
                info.board_position_override = info.animation_com_position;
                info.board_position_override[1] += rs::from_bits(0xbf4ccccdu);
                info.use_position_override = true;
                info.trajectory_count = 5;
                info.cone_angle_x *= rs::from_bits(0x3ecccccdu);
                info.cone_angle_z *= rs::from_bits(0x3ecccccdu);
            }
            info.player_jumped = false;
            S3G_TRY(const auto sel_input, input::selector_input(physics, skater));
            S3G_CHECK(skater.trajectory.launch(info, sel_input, physics.world));
            skater.air_state.trajectory_query_countdown = 7;
        }
    }
    S3G_TRY(const auto sel_input, input::selector_input(physics, skater));
    if (!skater.player_input.toolkit) {
        return rs::make_err(std::string("Air trajectory requires current board toolkit"));
    }
    const auto grind_context = air_trajectory::GrindContext::from_processed(skater.player_input.processed,
                                                                            skater.player_input.toolkit->deck[3]);
    S3G_CHECK(skater.trajectory.update(sel_input, physics.world, grind_context));
    const auto current = skater.air_reckoning.fields(physics.riding).current_landing_normal_1152;
    const auto normal = skater.trajectory.selector.suggested_normal().value_or(current);
    auto& state = skater.air_state;
    state.landing_normal = normal;
    const auto& settings = skater.air_settings.state;
    float blend = wrap_signed_angle(angle_between_vectors(normal, current)) <= settings.landing_normal_angle_limit_444
                      ? settings.landing_normal_blend_388
                      : 0.0f;
    if (!state.use_centre_of_mass_velocity) {
        if (!state.selector_latch_174) {
            state.selector_latch_174 = skater.trajectory.selector.all_predictions_missed();
        }
        if (state.selector_latch_174) {
            state.use_centre_of_mass_velocity = true;
            state.centre_of_mass_trajectory = AirTrajectory{
                frame.trajectory_position_592,
                frame.trajectory_velocity_608,
                COM_ACCELERATION,
                -1.0f,
            };
        }
    }
    if (frame.state_timer_2664 <= 0.0f) {
        blend = 0.0f;
    } else if ((frame.flags_2468 & 0x4000u) != 0) {
        state.use_centre_of_mass_velocity = false;
    }
    const float spin = (frame.body_spin_input_2640 * settings.body_spin_scale_428) * rs::from_bits(0x3c8efa35u);
    const float target_spin = settings.body_spin_over_time_320.evaluate(state.time_in_state * 2.0f) * spin;
    S3G_CHECK(skater.air_reckoning.update(physics.riding, skater.player_input.processed,
                                          skater.animation_input.extra.physical_body_spin, normal, blend,
                                          target_spin, 0.0f));
    if (skater.air_state.use_centre_of_mass_velocity) {
        integrate_trajectory_fixed_step(skater.air_state.centre_of_mass_trajectory);
        S3G_CHECK(skeleton::advance(physics, skater, skater.air_state.centre_of_mass_trajectory.position));
    } else {
        S3G_CHECK(skeleton::advance(physics, skater, std::nullopt));
    }
    S3G_CHECK(board::advance(physics, skater, frame));
    skater.skeleton_output.correction.pending = true;
    const float height = physics.board.part_transforms()[6].translation.y;
    auto& st = skater.air_state;
    if (!(st.max_y > height)) {
        st.max_y = height;
    }
    st.time_in_state += frame.delta_time_2604;
    return ok();
}

///82D34DD8. The common post-physics coordinator immediately follows this
///apex latch with Wipeout::CheckForAirWipeout(false), after collision feedback.
void update_apex(const GamePhysics& physics, PhysicsAirState& state) {
    if (!state.reached_apex && physics.board.bodies()[6].rates.linear_velocity.y < 0.0f) {
        state.reached_apex = true;
    }
}

}  // namespace s3::game::physics::air_phase
