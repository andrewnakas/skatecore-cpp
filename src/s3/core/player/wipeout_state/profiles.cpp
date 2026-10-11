// Port of skate-core/src/player/wipeout_state/profiles.rs (whole file).
#include "s3/core/player/wipeout_state/profiles.hpp"

namespace s3::player::wipeout_state::profiles {

void prepare(State& state, const std::array<Profile, 5>& profiles, std::array<float, 2> gesture,
             std::array<float, 2> input) {
    const float x = gesture[0];
    const float z = gesture[1];
    std::size_t selected;
    if (rs::abs(x) < 0.2f && rs::abs(z) < 0.2f) {
        selected = 0;
    } else if (rs::abs(x) <= rs::abs(z)) {
        selected = z <= 0.0f ? 3 : 1;
    } else if (x <= 0.0f) {
        selected = 4;
    } else {
        selected = 2;
    }
    state.control_time += rs::from_bits(0x3C888889u);
    if (selected != state.profile) {
        state.control_time = 0.0f;
        state.profile = selected;
        state.retained_sideways_input = 0.0f;
        state.retained_forward_input = 0.0f;
    }
    V flat_velocity = state.velocity;
    flat_velocity[1] = 0.0f;
    const V normalized = math::normalize_or(flat_velocity, V{});
    if (state.direction_initialized) {
        const float follow = profiles[selected].direction_follow;
        state.forward =
            math::normalize_or(math::madd(normalized, follow, math::scale(state.forward, 1.0f - follow)), V{});
    } else {
        state.direction_initialized = true;
        state.forward = normalized;
    }
    state.right = math::cross(V{0.0f, 1.0f, 0.0f, 0.0f}, state.forward);
    const V in{input[0], 0.0f, input[1], 0.0f};
    state.forward_input = math::dot(in, state.forward);
    state.sideways_input = math::dot(in, state.right);
}

void drift(const State& state, physics::skeleton_body::SkeletonBody& body, const Profile& p) {
    if (state.retained_velocity_active) {
        return;
    }
    const V desired = math::madd(state.right, p.drift_sideways * state.sideways_input,
                                 math::scale(state.forward, p.drift_forward * state.forward_input));
    V change = rs::map(state.velocity, [&](float v) { return rs::from_bits(rs::to_bits(v) ^ 0x80000000u) * p.drift_drag; });
    change[0] *= 0.02f;
    change[2] *= 0.02f;
    if (math::dot(state.velocity, math::normalize_or(desired, V{})) < p.drift_maximum_speed) {
        change = math::add(change, desired);
    }
    change = math::scale(change, rs::from_bits(0x3C888889u));
    auto& bodies = body.bodies_mut();
    for (std::size_t i = 1; i < 24; ++i) {
        bodies[i].rates.linear_velocity.x += change[0];
        bodies[i].rates.linear_velocity.y += change[1];
        bodies[i].rates.linear_velocity.z += change[2];
    }
}

}  // namespace s3::player::wipeout_state::profiles
