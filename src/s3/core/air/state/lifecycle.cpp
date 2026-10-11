// Port of skate-core/src/air/state/lifecycle.rs.
#include "s3/core/air/state/lifecycle.hpp"

namespace s3::air::state::lifecycle {

void exit(PhysicsAirState& state, PhysicsAirReckoningFields& reckoning) {
    state.use_centre_of_mass_velocity = false;
    reckoning.body_spin_speed_1572 = 0.0f;
    reckoning.body_spin_angle_1568 = 0.0f;
}

PhysicsAirOutput fill_physics_output(const PhysicsAirState& state) {
    PhysicsAirOutput out;
    out.is_at_apex = state.reached_apex;
    out.jump_height = state.max_y - state.start_y;
    out.landing_normal = state.landing_normal;
    out.scalar_184_write = state.selector_latch_174 ? std::optional<float>(OUTPUT_SCALAR_184) : std::nullopt;
    return out;
}

std::pair<bool, bool> initial_centre_of_mass_mode(const PhysicsAirFrame& frame) {
    if (frame.previous_physics_category_2516 == 400 || frame.previous_physics_state_2504 == 701) {
        return {(frame.flags_2468 & 0x00004000u) == 0, false};
    }
    const std::int32_t s = frame.previous_physics_state_2504;
    const bool selected = !(s == 100 || s == 103 || s == 201);
    return {selected, selected};
}

}  // namespace s3::air::state::lifecycle
