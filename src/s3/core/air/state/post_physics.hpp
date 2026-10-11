// Port of skate-core/src/air/state/post_physics.rs.
// TU3 PhysicsAir post-physics update (`0x82D34DD8`).
#pragma once
#include "s3/core/air/state/data.hpp"
#include "s3/core/air/state/runtime.hpp"

namespace s3::air::state::post_physics {

using data::PhysicsAirState;
using runtime::PhysicsAirRuntime;

/// Latches the first strict downward board velocity, then always runs the air
/// wipeout check. Negative zero and NaN do not mark the apex.
template <class L>
void update_post_physics(PhysicsAirState& state, PhysicsAirRuntime<L>& runtime) {
    if (!state.reached_apex) {
        const auto board_velocity = runtime.board_body_velocity();
        if (board_velocity[1] < 0.0f) {
            state.reached_apex = true;
        }
    }
    runtime.check_for_air_wipeout(false);
}

}  // namespace s3::air::state::post_physics
