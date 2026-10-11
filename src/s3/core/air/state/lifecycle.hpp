// Port of skate-core/src/air/state/lifecycle.rs.
// TU3 PhysicsAir Enter/Exit/output lifecycle.
#pragma once
#include <utility>

#include "s3/core/air/state/data.hpp"
#include "s3/core/air/state/jump_velocity.hpp"
#include "s3/core/air/state/runtime.hpp"

namespace s3::air::state::lifecycle {

using data::AirTrajectory;
using data::PhysicsAirFrame;
using data::PhysicsAirOutput;
using data::PhysicsAirReckoningFields;
using data::PhysicsAirState;
using data::Vector4;
using jump_velocity::calculate_velocity_from_jump;
using runtime::PhysicsAirRuntime;

inline constexpr float TRAJECTORY_UNBOUNDED = rs::from_bits(0xBF800000u);     // -1.0
inline constexpr float OFFBOARD_VERTICAL_SCALE = rs::from_bits(0x3F266666u);  // 0.65
inline constexpr float OFFBOARD_VERTICAL_LIMIT = rs::from_bits(0x40400000u);  // 3.0
inline constexpr float OUTPUT_SCALAR_184 = rs::from_bits(0x40800000u);        // 4.0

/// Module-private in Rust. Returns (use_centre_of_mass_velocity, request_heading_update).
std::pair<bool, bool> initial_centre_of_mass_mode(const PhysicsAirFrame& frame);

/// Full TU3 `PhysState_PhysicsAir::Enter`, `0x82D34388`.
template <class L>
void enter(PhysicsAirState& state, const PhysicsAirFrame& frame, Vector4 world_acceleration,
           PhysicsAirRuntime<L>& runtime) {
    runtime.set_air_collision_update_enabled(true);
    runtime.set_skeleton_collision_state(7);
    runtime.set_skeleton_inverse_kinematics_enabled(true);
    runtime.set_skeleton_collision_state(6);
    runtime.enable_board_angular_drive_only();
    runtime.set_footplant_flag_240(false);
    runtime.reset_footplants();

    state.selector_latch_174 = false;
    state.reached_apex = false;
    state.trajectory_query_countdown = 0;
    state.landing_normal = {0.0f, 1.0f, 0.0f, 0.0f};
    state.time_in_state = 0.0f;
    state.max_y = runtime.board_transform_height();
    state.start_y = frame.ground_position_y_500;

    const auto [use_centre_of_mass_velocity, request_heading_update] = initial_centre_of_mass_mode(frame);
    state.use_centre_of_mass_velocity = use_centre_of_mass_velocity;

    if (use_centre_of_mass_velocity) {
        Vector4 velocity = frame.trajectory_velocity_608;
        if (frame.previous_physics_state_2504 == 500) {
            const float scaled_y = velocity[1] * OFFBOARD_VERTICAL_SCALE;
            velocity[1] = runtime.minimum_vminfp(scaled_y, OFFBOARD_VERTICAL_LIMIT);
        }
        if (frame.frames_since_jump_correction_2576 < 12) {
            velocity = calculate_velocity_from_jump(frame, frame.frames_since_jump_correction_2576, runtime);
        }
        state.centre_of_mass_trajectory = AirTrajectory{
            frame.trajectory_position_592,
            velocity,
            world_acceleration,
            TRAJECTORY_UNBOUNDED,
        };
    }

    // This branch-local flag deliberately differs from the COM selection in
    // the grind/category-400 path.
    if (request_heading_update) {
        runtime.request_skeleton_heading_update();
    }
}

/// TU3 `PhysState_PhysicsAir::Exit`, `0x82D346A0`.
void exit(PhysicsAirState& state, PhysicsAirReckoningFields& reckoning);

/// TU3 `PhysState_PhysicsAir::FillPhysOut`, `0x82D34E90`.
PhysicsAirOutput fill_physics_output(const PhysicsAirState& state);

}  // namespace s3::air::state::lifecycle
