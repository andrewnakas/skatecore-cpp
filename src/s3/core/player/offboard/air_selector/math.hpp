// Port of skate-core/src/player/offboard/air_selector/math.rs (whole file).
// Reuse ordinary native-arithmetic leaves; no new physical solver.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/player/_deps/air_trajectory.hpp"
#include "s3/core/player/wipeout_state/math.hpp"

namespace s3::player::offboard::air_selector::math {

using Vector = std::array<float, 4>;
using air::trajectory::Trajectory;
using wipeout_state::math::add;
using wipeout_state::math::cross;
using wipeout_state::math::dot;
using wipeout_state::math::length;
using wipeout_state::math::madd;
using wipeout_state::math::normalize;
using wipeout_state::math::normalize_or;
using wipeout_state::math::reciprocal;
using wipeout_state::math::scale;
using wipeout_state::math::sub;
using wipeout_state::math::wrap_angle;

inline const float DT_ = rs::from_bits(0x3c888889u);
inline constexpr Vector UP{0.0f, 1.0f, 0.0f, 0.0f};

inline Vector flatten(Vector v) {
    v[1] = 0.0f;
    return v;
}
inline void shift(Trajectory& t, float time) {
    const Vector p = t.position_at(time);
    t.velocity = t.velocity_at(time);
    t.position = p;
}
///82D609E0: velocity correction only, bounded at the caller's maximum.
inline void adjust(Trajectory& t, std::int32_t frame, Vector delta, float maximum) {
    if (frame <= 0) {
        return;
    }
    const Vector correction = scale(delta, reciprocal(static_cast<float>(frame) * DT_));
    const float scalar = dot(correction, correction) > maximum * maximum ? maximum / length(correction) : 1.0f;
    t.velocity = madd(correction, scalar, t.velocity);
}

}  // namespace s3::player::offboard::air_selector::math
