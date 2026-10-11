// Port of skate-core/src/player/offboard/air_launch/math.rs (whole file).
// Recovered leaf expressions. Ordinary PC estimates retain native refinements;
// they are not claimed to be bit-exact Xenon arithmetic. pub(in crate::player::offboard).
#pragma once
#include <array>

#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/player/wipeout_state/math.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::player::offboard::air_launch::math {

using Vector = std::array<float, 4>;
using wipeout_state::math::add;
using wipeout_state::math::clamp;
using wipeout_state::math::cross;
using wipeout_state::math::dot;
using wipeout_state::math::length;
using wipeout_state::math::madd;
using wipeout_state::math::normalize;
using wipeout_state::math::normalize_or;
using wipeout_state::math::scale;
using wipeout_state::math::sub;
using wipeout_state::math::wrap_angle;

inline constexpr Vector ZERO{0.0f, 0.0f, 0.0f, 0.0f};
inline constexpr Vector UP{0.0f, 1.0f, 0.0f, 0.0f};
inline const float DT = rs::from_bits(0x3c888889u);
inline const float RADIANS = rs::from_bits(0x3c8efa35u);

inline float select(float test, float positive, float negative) { return test >= 0.0f ? positive : negative; }
inline Vector flatten(Vector v) {
    v[1] = 0.0f;
    return v;
}
inline float magnitude(float q) {
    const float value = q * physics::board_motion_output::inverse_length_squared(q, 2);
    return q == 0.0f ? 0.0f : value;
}
///82E0A4E8: normalize axis safely, then separate multiplication/subtraction.
inline Vector reject(Vector v, Vector axis) {
    axis = normalize_or(axis, ZERO);
    return sub(v, scale(axis, dot(v, axis)));
}
///82BD3E78, including its strict angle comparison and raw quaternion W insert.
inline Vector limit_angle(Vector target, Vector from, float maximum) {
    const Vector a = normalize_or(target, ZERO);
    const Vector b = normalize_or(from, ZERO);
    const Vector axis = cross(a, b);
    const float angle = trigonometry::acos(rs::min(rs::max(dot(a, b), -1.0f), 1.0f));
    const float turns = rs::floor(rs::mul_add(angle, rs::from_bits(0x3e22f983u), 0.5f));
    const float closest = rs::mul_add(-turns, rs::from_bits(0x40c90fdbu), angle);
    if (rs::abs(closest) < maximum || dot(axis, axis) < rs::from_bits(0x37800000u)) {
        return target;
    }
    const auto [s, c] = trigonometry::sin_cos(-maximum * 0.5f);
    Vector q = scale(normalize(axis), s);
    q[3] = c;
    const Vector first = cross(q, from);
    const Vector middle = madd(from, c, first);
    return scale(madd(cross(q, middle), 2.0f, from), length(target));
}
///82D7C724..C7F4 XYZ Rodrigues rotation. Host representation adaptation:
///native permutation scratch lanes are not a fourth spatial velocity component.
///Keep the geometric vector W=0 at this producer, before trajectory feedback.
inline Vector rotate(Vector v, Vector axis, float angle) {
    const float x = axis[0], y = axis[1], z = axis[2];
    const auto [s, c] = trigonometry::sin_cos(angle);
    const float t = 1.0f - c;
    const float tx = t * x, ty = t * y, tz = t * z;
    const float sx = s * x, sy = s * y, sz = s * z;
    const float xx = rs::mul_add(tx, x, c);
    const float yx = ty * x - sz;
    const float zx = rs::mul_add(tz, x, sy);
    const Vector right{xx, rs::mul_add(tx, y, sz), tx * z - sy, 0.0f};
    const Vector up{yx, rs::mul_add(ty, y, c), rs::mul_add(ty, z, sx), 0.0f};
    const Vector forward{zx, tz * y - sx, rs::mul_add(tz, z, c), 0.0f};
    return madd(forward, v[2], madd(up, v[1], scale(right, v[0])));
}

}  // namespace s3::player::offboard::air_launch::math
