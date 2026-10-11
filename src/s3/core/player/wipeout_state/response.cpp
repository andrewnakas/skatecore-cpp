// Port of skate-core/src/player/wipeout_state/response.rs (whole file).
#include "s3/core/player/wipeout_state/response.hpp"

#include "s3/core/player/wipeout_state/body.hpp"

namespace s3::player::wipeout_state::response {

using physics::skeleton_body::SkeletonBody;
using physics::skeleton_body::SkeletonDrives;

V response_normal(std::optional<V> support, std::optional<V> prediction) {
    if (support && (*support)[1] > 0.0f) {
        return *support;
    }
    if (prediction && (*prediction)[1] > 0.0f) {
        return *prediction;
    }
    return V{0.0f, 1.0f, 0.0f, 0.0f};
}

void trigger(State& state, SkeletonBody& body, V normal, std::array<float, 2> input) {
    state.response_frames = 0;
    state.response_start_speed = math::length(state.velocity);
    float strength = state.response_strength();
    if (state.maximum_speed * 0.1f < 1.0f) {
        state.response_count = state.response_count + 1u;
    } else {
        state.response_count = 0;
    }
    state.maximum_speed = 0.0f;
    state.extra_weight = 0.0f;
    state.response_scalar = 0.0f;
    if (strength == 0.0f) {
        return;
    }
    state.response_time = 0.0f;
    strength = strength - 1.0f >= 0.0f ? 1.0f : strength;
    state.response_scalar = strength;
    state.extra_weight = strength;
    V control{input[0], 0.0f, input[1], 0.0f};
    const float projection = math::dot(normal, control);
    if (projection < 0.0f) {
        control = math::sub(control, math::scale(normal, projection));
    }
    // 82D3E868: raw vmadd A=normal,C=zero,B=control*1.25.
    const V velocity = math::scale(math::madd(normal, 0.0f, math::scale(control, 1.25f)), strength);
    body::add_velocity(body, velocity);
}

void update_counter(State& state, bool contact) {
    state.response_finished = false;
    if (state.response_frames < 0) {
        return;
    }
    state.response_frames = static_cast<std::int32_t>(static_cast<std::uint32_t>(state.response_frames) + 1u);
    if (state.response_frames == 3) {
        const float factor =
            state.response_start_speed < 1.0f ? 1.0f - state.response_start_speed : state.response_start_speed * 0.2f;
        state.response_change = (math::length(state.velocity) - state.response_start_speed) * (factor + 1.0f);
    }
    if (state.response_frames == 8) {
        state.response_frames = -1;
        state.response_finished = true;
        if (!contact) {
            state.response_change = rs::abs(state.response_change) * 2.0f;
        }
        state.response_change = math::clamp((state.response_change - 0.1f) * 0.2f, 0.0f, 1.0f);
    }
}

namespace {
void release(State& state, SkeletonBody& body, SkeletonDrives& targets, const drives::Settings& settings) {
    if (!state.retained_velocity_active) {
        return;
    }
    drives::set_linear_root(targets, settings, 0.0f);
    body::set_velocity(body, state.retained_velocity);
    state.retained_velocity_active = false;
    state.velocity = V{};
}
}  // namespace

void retained_velocity(State& state, SkeletonBody& body, SkeletonDrives& targets, const drives::Settings& settings,
                       bool contact, std::uint32_t flags2468, V com_velocity) {
    const bool requested = (flags2468 & 8u) != 0;
    if (state.retained_velocity_active || requested) {
        if (contact) {
            state.allow_retained_velocity = false;
            release(state, body, targets, settings);
        } else if (!requested) {
            release(state, body, targets, settings);
        } else if (!state.retained_velocity_active && state.allow_retained_velocity && com_velocity[1] < 2.0f) {
            drives::set_linear_root(targets, settings, 1.0f);
            state.retained_velocity_active = true;
            state.retained_velocity = state.velocity;
        }
    }
    if (state.retained_velocity_active) {
        state.velocity = state.retained_velocity;
        state.time -= rs::from_bits(0x3C888889u);
    }
}

}  // namespace s3::player::wipeout_state::response
