// Port of skate-core/src/air/known/update.rs.
#include "s3/core/air/known/update.hpp"

#include "s3/core/air/known/body.hpp"
#include "s3/core/air/known/footplant.hpp"
#include "s3/core/air/known/trajectory.hpp"

namespace s3::air::known::update_mod {

using body::calculate_body_flip_speed;
using body::calculate_body_spin_speed;
using footplant::update_footplant;
using trajectory::init_trajectory_info;
using trajectory::select;
using trajectory::update_trajectory_follow;

namespace {
constexpr float ALIGNMENT_LEAD_FRAMES = rs::from_bits(0x40400000u);  // 0x82063B08, 3.
constexpr float MIN_ALIGNMENT_FRAMES = rs::from_bits(0x40000000u);   // 0x82060C50, 2.
constexpr float ONE = rs::from_bits(0x3F800000u);                    // 0x8231A844.
constexpr float BOARD_ERROR_WINDOW = rs::from_bits(0x3D75C28Fu);     // 0x82099460, 0.06.
constexpr float ZERO = 0.0f;                                         // 0x82165A10.
}  // namespace

void update(KnownAirState& state, const KnownAirFrame& frame, const KnownAirSettings& settings,
            const KnownAirModeSettings& mode, const KnownAirReckoningFields& reckoning, KnownAirRuntime& runtime) {
    update_footplant(state, frame, runtime);
    if (runtime.selector_has_just_changed()) {
        init_trajectory_info(state, frame, settings, mode, runtime);
    }
    runtime.update_air_collision();

    const float body_flip_speed = calculate_body_flip_speed(state, frame, settings, mode, reckoning, runtime);
    const float body_spin_speed = calculate_body_spin_speed(state, frame, settings, mode, reckoning, runtime);

    const float remaining_frames = (state.collision_time_196 - state.time_in_state_180) / frame.delta_time_2604;
    const float after_lead = remaining_frames - ALIGNMENT_LEAD_FRAMES;
    const float alignment_frames = select(after_lead - MIN_ALIGNMENT_FRAMES, after_lead, MIN_ALIGNMENT_FRAMES);

    if (!state.grind_air_adjust_activated_212 &&
        (state.reached_apex_208 || alignment_frames < settings.frames_for_grind_air_assist_436)) {
        const bool started = runtime.grind_air_adjust_started();
        runtime.set_grind_air_adjust_activated(started);
        state.grind_air_adjust_activated_212 = true;
    }

    if (state.targeting_grind_213 && state.grind_air_adjust_activated_212 && runtime.grind_air_adjust_adjusting()) {
        for (const GrindFootBody body : GRIND_FOOT_BODIES) {
            runtime.disable_grind_foot_collision(body);
        }
    }

    runtime.update_reckoning_air_states(state.landing_normal_64, ONE / alignment_frames, body_spin_speed,
                                        body_flip_speed);
    update_trajectory_follow(state, frame, settings, runtime);
    runtime.update_known_air_skeleton(state.target_com_position_160);

    if (state.time_in_state_180 < BOARD_ERROR_WINDOW) {
        runtime.set_skeleton_add_skateboard_error(true);
    }

    const auto head_target =
        (frame.flags_2472 & 0x00400000u) != 0 ? frame.alternate_head_target_112 : state.collision_position_112;
    runtime.set_head_tracking_target(head_target, true);
    runtime.update_board_steering_tilt(ZERO);

    const float board_y = runtime.board_transform_height();
    if (!(state.max_y_188 > board_y)) {
        state.max_y_188 = board_y;
    }
    if ((frame.flags_2468 & 0x00000008u) == 0) {
        state.time_in_state_180 += frame.delta_time_2604;
    }

    const float com_y = runtime.skeleton_com_position()[1];
    state.com_max_y_192 = select(state.com_max_y_192 - com_y, state.com_max_y_192, com_y);
}

}  // namespace s3::air::known::update_mod
