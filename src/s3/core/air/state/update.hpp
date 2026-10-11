// Port of skate-core/src/air/state/update.rs (module air::state::update; C++ namespace `update_mod`
// because the re-exported fn `state::update` would clash with a same-named namespace).
// TU3 PhysicsAir Update (`0x82D346C0`) and COM launch helper (`0x82D34570`).
#pragma once
#include <cstdint>

#include "s3/core/air/state/board.hpp"
#include "s3/core/air/state/data.hpp"
#include "s3/core/air/state/jump_velocity.hpp"
#include "s3/core/air/state/runtime.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::air::state::update_mod {

using board::update_skateboard;
using data::AirTrajectory;
using data::PhysicsAirFrame;
using data::PhysicsAirReckoningFields;
using data::PhysicsAirSettings;
using data::PhysicsAirState;
using data::Vector4;
using jump_velocity::calculate_velocity_from_jump;
using point_graph::PointGraph;
using runtime::PhysicsAirLaunchInfo;
using runtime::PhysicsAirRuntime;

// Module-private constants in Rust.
inline constexpr float FIXED_STEP = rs::from_bits(0x3C888889u);  // 1/60, 0x82098D40
inline constexpr float HALF = rs::from_bits(0x3F000000u);
inline constexpr float ONE = rs::from_bits(0x3F800000u);
inline constexpr float TRAJECTORY_UNBOUNDED = rs::from_bits(0xBF800000u);
inline constexpr float TWO = rs::from_bits(0x40000000u);
inline constexpr float DEGREES_TO_RADIANS = rs::from_bits(0x3C8EFA35u);
inline constexpr float TWO_PI = rs::from_bits(0x40C90FDBu);
inline constexpr float RECIPROCAL_TWO_PI = rs::from_bits(0x3E22F983u);
inline constexpr float HALF_TURN = rs::from_bits(0x3F000000u);
inline constexpr float COM_START_Y_OFFSET = rs::from_bits(0xBF266666u);    // -0.65
inline constexpr float BOARD_START_Y_OFFSET = rs::from_bits(0xBF4CCCCDu);  // -0.8
inline constexpr float CONE_SCALE = rs::from_bits(0x3ECCCCCDu);            // 0.4

/// Scalar wrap at `0x82D347E8..0x82D34848`, after AngleBetweenVectors.
float wrap_signed_angle(float angle);

/// Fixed 1/60 ballistic step at `0x82D349B8..0x82D34A20`.
void integrate_trajectory_fixed_step(AirTrajectory& trajectory);

/// Module-private in Rust.
inline float evaluate_spin_curve(const PointGraph<8>& graph, float input) { return graph.evaluate(input); }

/// Module-private in Rust.
template <class L>
void adjust_centre_of_mass_launch_info(const PhysicsAirState& state, Vector4 world_acceleration,
                                       PhysicsAirRuntime<L>& runtime, L& info) {
    runtime.fill_skeleton_launch_info(info);

    Vector4 velocity{};
    for (std::size_t lane = 0; lane < 4; ++lane) {
        velocity[lane] = rs::mul_add(world_acceleration[lane], FIXED_STEP, state.centre_of_mass_trajectory.velocity[lane]);
    }
    info.set_start_velocity(velocity);
    info.set_player_jumped(true);

    const Vector4 animation_position = info.centre_of_mass_animation_position();
    const Vector4 trajectory_offset{0.0f, COM_START_Y_OFFSET, 0.0f, 0.0f};
    const Vector4 board_offset{0.0f, BOARD_START_Y_OFFSET, 0.0f, 0.0f};
    Vector4 trajectory_override{};
    Vector4 board_override{};
    for (std::size_t lane = 0; lane < 4; ++lane) {
        trajectory_override[lane] = animation_position[lane] + trajectory_offset[lane];
        board_override[lane] = animation_position[lane] + board_offset[lane];
    }
    info.set_trajectory_start_position_override(trajectory_override);
    info.set_board_position_override(board_override);
    info.set_use_trajectory_start_position_override(true);
    info.set_trajectory_count(5);

    const auto cone_angles = info.cone_angles();
    info.set_cone_angles({cone_angles[0] * CONE_SCALE, cone_angles[1] * CONE_SCALE});
}

/// Module-private in Rust.
template <class L>
void launch_trajectory(const PhysicsAirState& state, const PhysicsAirFrame& frame, Vector4 world_acceleration,
                       PhysicsAirRuntime<L>& runtime) {
    L info = runtime.construct_trajectory_launch_info();
    runtime.fill_skeleton_launch_info(info);
    if (frame.frames_since_jump_correction_2576 < 12) {
        const auto velocity = calculate_velocity_from_jump(frame, frame.frames_since_jump_correction_2576, runtime);
        info.set_start_velocity(velocity);
    } else if (state.use_centre_of_mass_velocity) {
        adjust_centre_of_mass_launch_info(state, world_acceleration, runtime, info);
    }

    // The caller always clears +268, including after the COM helper sets it.
    info.set_player_jumped(false);
    runtime.launch_trajectory(info);
}

/// Full recovered outer update, retaining native effect order.
template <class L>
void update(PhysicsAirState& state, const PhysicsAirFrame& frame, const PhysicsAirSettings& settings,
            PhysicsAirReckoningFields& reckoning, Vector4 world_acceleration, PhysicsAirRuntime<L>& runtime) {
    runtime.update_air_collision();

    state.trajectory_query_countdown =
        static_cast<std::int32_t>(static_cast<std::uint32_t>(state.trajectory_query_countdown) - 1u);
    if (state.trajectory_query_countdown <= 0 && (frame.flags_2468 & 0x00008000u) == 0 &&
        frame.previous_physics_state_2504 != 0 && !runtime.trajectory_query_just_started()) {
        launch_trajectory(state, frame, world_acceleration, runtime);
        state.trajectory_query_countdown = 7;
    }

    runtime.update_trajectory_selector();

    const Vector4 current_normal = reckoning.current_landing_normal_1152;
    const Vector4 landing_normal = runtime.selector_landing_normal().value_or(current_normal);
    state.landing_normal = landing_normal;

    const float raw_angle = runtime.angle_between_vectors(landing_normal, current_normal);
    const float wrapped_angle = wrap_signed_angle(raw_angle);
    float normal_blend =
        wrapped_angle <= settings.landing_normal_angle_limit_444 ? settings.landing_normal_blend_388 : 0.0f;

    if (!state.use_centre_of_mass_velocity) {
        if (!state.selector_latch_174) {
            state.selector_latch_174 = runtime.selector_centre_of_mass_trajectory_ready();
        }
        if (state.selector_latch_174) {
            state.use_centre_of_mass_velocity = true;
            state.centre_of_mass_trajectory = AirTrajectory{
                frame.trajectory_position_592,
                frame.trajectory_velocity_608,
                world_acceleration,
                TRAJECTORY_UNBOUNDED,
            };
        }
    }

    if (frame.state_timer_2664 <= 0.0f) {
        normal_blend = 0.0f;
    } else if ((frame.flags_2468 & 0x00004000u) != 0) {
        state.use_centre_of_mass_velocity = false;
    }

    const float scaled_spin_input = frame.body_spin_input_2640 * settings.body_spin_scale_428;
    const float spin_radians = scaled_spin_input * DEGREES_TO_RADIANS;
    const float curve = evaluate_spin_curve(settings.body_spin_over_time_320, state.time_in_state * TWO);
    const float target_body_spin = curve * spin_radians;
    reckoning = runtime.update_reckoning_air_states(landing_normal, normal_blend, target_body_spin, 0.0f);

    if (state.use_centre_of_mass_velocity) {
        integrate_trajectory_fixed_step(state.centre_of_mass_trajectory);
        runtime.update_known_air_skeleton(state.centre_of_mass_trajectory.position);
    } else {
        runtime.update_animated_skateboard_skeleton(false);
    }

    update_skateboard(state, frame, reckoning, runtime);
    runtime.enable_skateboard_error_on_skeleton();

    const float board_y = runtime.board_transform_height();
    // fcmpu+bgt keeps the old value only for an ordered old > new result;
    // unordered values take the board-height write.
    if (!(state.max_y > board_y)) {
        state.max_y = board_y;
    }
    state.time_in_state += frame.delta_time_2604;
}

}  // namespace s3::air::state::update_mod
