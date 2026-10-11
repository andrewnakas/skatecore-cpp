// Port of skate-core/src/air/state/board.rs.
// TU3 PhysicsAir board update (`0x82D34AB0`).
#pragma once
#include <cstdint>

#include "s3/core/air/state/data.hpp"
#include "s3/core/air/state/jump_velocity.hpp"
#include "s3/core/air/state/runtime.hpp"

namespace s3::air::state::board {

using data::AirBoardForce;
using data::PhysicsAirFrame;
using data::PhysicsAirReckoningFields;
using data::PhysicsAirState;
using jump_velocity::calculate_velocity_from_jump;
using runtime::PhysicsAirRuntime;

inline constexpr std::uint32_t AIR_COLLISION_FORCE_TAG = 15;

template <class L>
void update_skateboard(const PhysicsAirState& state, const PhysicsAirFrame& frame,
                       const PhysicsAirReckoningFields& reckoning, PhysicsAirRuntime<L>& runtime) {
    runtime.update_board_steering_tilt(0.0f);

    AirBoardForce force = AirBoardForce::ZERO;
    const bool collision_force_created =
        runtime.calculate_air_collision_force(reckoning.collision_reference_normal_1216, force);
    if (collision_force_created) {
        runtime.enqueue_board_force(AIR_COLLISION_FORCE_TAG, force);
        return;
    }

    if (!state.use_centre_of_mass_velocity && frame.frames_since_jump_correction_2576 < 12) {
        const auto velocity = calculate_velocity_from_jump(frame, frame.frames_since_jump_correction_2576, runtime);
        runtime.set_board_velocity(velocity);
    }
}

}  // namespace s3::air::state::board
