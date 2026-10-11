// Port of skate-core/src/air/known/footplant.rs.
#include "s3/core/air/known/footplant.hpp"

namespace s3::air::known::footplant {

namespace {
constexpr float MIN_FOOTPLANT_TIME = rs::from_bits(0x3DCCCCCDu);  // 0x820641A8.
}

void update_footplant(const data::KnownAirState& state, const data::KnownAirFrame& frame,
                      runtime::KnownAirRuntime& runtime) {
    if ((frame.flags_2480 & 0x04000000u) == 0 || (frame.flags_2476 & 0x04000000u) != 0 ||
        state.time_in_state_180 <= MIN_FOOTPLANT_TIME) {
        runtime.reset_footplants();
        return;
    }

    data::KnownAirFootplantInput input;
    input.trajectory = runtime.selector_trajectory_2704();
    input.remaining_collision_time = state.collision_time_196 - state.time_in_state_180;
    input.collision_position = state.collision_position_112;
    input.landing_normal = state.landing_normal_64;
    runtime.shift_footplant_trajectory_to_index(input.trajectory, state.trajectory_index_216);
    runtime.update_footplant_prediction(input);
    runtime.update_footplant_lock(input);
    runtime.update_footplant_pose(input);
}

}  // namespace s3::air::known::footplant
