// Port of skate-core/src/air/known/lifecycle.rs.
#include "s3/core/air/known/lifecycle.hpp"

#include "s3/core/air/known/trajectory.hpp"

namespace s3::air::known::lifecycle {

using trajectory::init_trajectory_info;
using trajectory::restore_velocity;

namespace {
constexpr data::Vector4 ZERO_VECTOR{0.0f, 0.0f, 0.0f, 0.0f};  // 0x830BD350.
}

void enter(KnownAirState& state, const KnownAirFrame& frame, const KnownAirSettings& settings,
           const KnownAirModeSettings& mode, KnownAirRuntime& runtime) {
    runtime.enable_board_angular_drive_only();
    runtime.set_air_collision_update_enabled(true);
    runtime.set_skeleton_collision_state(7);
    runtime.set_skeleton_physics_truck_tilt_enabled(false);
    runtime.set_skeleton_inverse_kinematics_enabled(true);

    state.body_flipping_211 = false;
    state.grind_air_adjust_activated_212 = false;
    state.targeting_grind_213 = false;
    runtime.reset_reckoning_flipping();
    runtime.set_footplant_flag_240(false);
    runtime.reset_footplants();

    state.reached_apex_208 = false;
    state.time_in_state_180 = 0.0f;
    state.start_y_184 = frame.start_height_484;
    state.max_y_188 = runtime.board_transform_height();
    state.com_max_y_192 = runtime.skeleton_com_position()[1];

    init_trajectory_info(state, frame, settings, mode, runtime);

    if (runtime.selector_targeting_grind()) {
        runtime.start_grind_air_adjust_from_selector();
        state.targeting_grind_213 = true;
    }

    const auto skeleton_com = runtime.skeleton_com_position();
    const auto [index, offset] = runtime.selector_closest_trajectory_point(skeleton_com, ZERO_VECTOR);
    state.trajectory_index_216 = index;
    state.trajectory_follow_offset_144 = offset;

    state.start_flipped_210 = 0.0f > runtime.dot3(frame.velocity_400, frame.start_flip_reference_96);
}

void exit(KnownAirState& state, KnownAirFrame& frame, const KnownAirSettings& settings,
          KnownAirReckoningFields& reckoning, KnownAirRuntime& runtime) {
    runtime.reset_reckoning_flipping();
    if (frame.next_physics_state_2500 == 100) {
        restore_velocity(state, frame, settings, runtime);
    }
    runtime.reset_trajectory_selector();
    reckoning.body_spin_speed_1572 = 0.0f;
    reckoning.body_spin_angle_1568 = 0.0f;
    runtime.set_grind_air_adjust_started(false);
    runtime.set_grind_air_adjust_activated(false);
}

}  // namespace s3::air::known::lifecycle
