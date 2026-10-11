// Port of skate-core/src/player/wipeout/common.rs (whole file).
#pragma once
#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/player/wipeout/data.hpp"
#include "s3/core/player/wipeout/requests.hpp"

namespace s3::player::wipeout::common {

// pub(super)
inline float length(std::array<float, 4> v) {
    const float sq = physics::native_arithmetic::dot3(v, v);
    const float inv = physics::board_motion_output::inverse_length_squared(sq, 2);
    return sq == 0.0f ? 0.0f : sq * inv;
}
inline float selected_max(float a, float b) { return a - b >= 0.0f ? a : b; }
/// 82BD88A0 reads unweighted regional forces in this exact pair order.
inline bool force(const Frame& frame, float body, float arms) {
    const auto& f = frame.regions_force;
    const float a = selected_max(f[0], f[1]);
    const float b = selected_max(f[5], f[4]);
    const float c = selected_max(f[7], f[6]);
    return selected_max(selected_max(a, b), c) > body || selected_max(f[2], f[3]) > arms;
}
// pub(super)
inline void vehicle(Requests& state, const Settings& s, const Frame& f) {
    const float limit = (f.flags_2476 & 8u) != 0 ? s.ground.skitch_contact : s.ground.vehicle_contact;
    if (f.vehicle_force > limit) {
        state.request(7, 0.0f);
    }
}
/// 82D90AB8 rotates closing velocity656 by the actual WorldToAnim basis.
inline void closing(Requests& state, const Frame& f, float xz, float y) {
    if (!f.board_contact) {
        return;
    }
    const auto v = f.closing_velocity;
    const auto& m = f.world_to_animation;
    std::array<float, 4> local{};
    for (std::size_t i = 0; i < 4; ++i) {
        local[i] = rs::mul_add(m[2][i], v[2], rs::mul_add(m[1][i], v[1], m[0][i] * v[0]));
    }
    if (length({local[0], 0.0f, local[2], local[3]}) > xz || rs::abs(local[1]) > y) {
        state.request(2, 0.0f);
        if (force(f, 1.0f, 20.0f)) {
            state.request(0, 0.0f);
        }
    }
}
// pub(super)
inline void leaning(Requests& state, const Settings& s, const Frame& f) {
    if (f.animation_up[1] < s.lean_contact_y && f.compliant &&
        f.highest_normal[1] > rs::from_bits(0x3f34fdf4u)) {
        state.request(19, 0.0f);
    }
}

}  // namespace s3::player::wipeout::common
