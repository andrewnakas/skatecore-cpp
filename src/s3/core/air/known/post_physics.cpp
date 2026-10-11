// Port of skate-core/src/air/known/post_physics.rs.
#include "s3/core/air/known/post_physics.hpp"

namespace s3::air::known::post_physics {

void update_post_physics(KnownAirState& state, const KnownAirFrame& frame, const KnownAirModeSettings& mode,
                         const KnownAirWipeoutSettings& settings, KnownAirWipeoutRequest& request,
                         KnownAirRuntime& runtime) {
    if (!state.reached_apex_208) {
        const auto velocity = runtime.board_body_velocity();
        if (velocity[1] < 0.0f) {
            state.reached_apex_208 = true;
        }
    }
    runtime.check_for_air_wipeout(true);
    check_upside_down_falling_wipeout(state, frame, mode, settings, request, runtime);
}

void check_upside_down_falling_wipeout(const KnownAirState& state, const KnownAirFrame& frame,
                                       const KnownAirModeSettings& mode, const KnownAirWipeoutSettings& settings,
                                       KnownAirWipeoutRequest& request, KnownAirRuntime& runtime) {
    if (!mode.upside_down_falling_wipeout_enabled_2) {
        return;
    }
    if ((frame.flags_2468 & 0x00000020u) != 0) {
        return;
    }
    if (!(frame.skater_up_544[1] < settings.air_falling_min_up_y_260)) {
        return;
    }
    if (runtime.dot3(state.landing_normal_64, frame.skater_up_544) < settings.air_falling_max_angle_264) {
        request.requested_35 = true;
        request.scalar_116 = 0.0f;
        request.counter_200 = request.counter_200 + 1u;  // wrapping_add (u32)
    }
}

}  // namespace s3::air::known::post_physics
