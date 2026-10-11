// Port of skate-core/src/air/state/update.rs.
#include "s3/core/air/state/update.hpp"

namespace s3::air::state::update_mod {

float wrap_signed_angle(float angle) {
    const float turns = angle * RECIPROCAL_TWO_PI;
    const float fraction = turns - rs::floor(turns);
    const float signed_fraction = fraction > HALF_TURN ? fraction - ONE : fraction;
    return signed_fraction * TWO_PI;
}

void integrate_trajectory_fixed_step(AirTrajectory& trajectory) {
    const float step_squared = FIXED_STEP * FIXED_STEP;
    Vector4 linear_position{};
    Vector4 next_velocity{};
    for (std::size_t lane = 0; lane < 4; ++lane) {
        linear_position[lane] = rs::mul_add(trajectory.velocity[lane], FIXED_STEP, trajectory.position[lane]);
    }
    for (std::size_t lane = 0; lane < 4; ++lane) {
        next_velocity[lane] = rs::mul_add(trajectory.acceleration[lane], FIXED_STEP, trajectory.velocity[lane]);
    }
    const float half = ONE * HALF;
    Vector4 next_position{};
    for (std::size_t lane = 0; lane < 4; ++lane) {
        const float half_acceleration = trajectory.acceleration[lane] * half;
        next_position[lane] = rs::mul_add(half_acceleration, step_squared, linear_position[lane]);
    }
    trajectory.position = next_position;
    trajectory.velocity = next_velocity;
}

}  // namespace s3::air::state::update_mod
