// Port of skate-core/src/air/reckoning/math.rs.
#include "s3/core/air/reckoning/math.hpp"

#include "s3/core/math.hpp"
#include "s3/core/physics/board_ground.hpp"
#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::air::reckoning::math {

using physics::board_ground::angle_between;
using physics::board_motion_output::inverse_length_squared;
using physics::native_arithmetic::dot3;
using physics::native_arithmetic::vector_max;
using physics::native_arithmetic::vector_min;

namespace {
const float EPSILON = rs::from_bits(0x358637bdu);
const float TAU = rs::from_bits(0x40c90fdbu);
const float INV_TAU = rs::from_bits(0x3e22f983u);

V cross(V a, V b) {
    return V{
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
        rs::mul_add(-a[3], b[3], a[3] * b[3]),
    };
}
V map_scale(V v, float s) { return V{v[0] * s, v[1] * s, v[2] * s, v[3] * s}; }
}  // namespace

float angle(V a, V b) {
    return angle_between(s3::math::Vector3::new_(a[0], a[1], a[2]), s3::math::Vector3::new_(b[0], b[1], b[2]));
}

float wrap(float angle) {
    const float turns = angle * INV_TAU;
    const float fraction = turns - rs::floor(turns);
    return (fraction - (fraction > 0.5f ? 1.0f : 0.0f)) * TAU;
}

float length(V v) {
    const float sq = dot3(v, v);
    const float inverse = inverse_length_squared(sq, 2);
    return sq == 0.0f ? 0.0f : sq * inverse;
}

V normalize(V v) {
    const float inverse = inverse_length_squared(dot3(v, v), 2);
    return map_scale(v, inverse);
}

V normalize_safe(V v, V fallback) {
    const float sq = dot3(v, v);
    const float inverse = inverse_length_squared(sq, 2);
    const float magnitude = sq == 0.0f ? 0.0f : sq * inverse;
    if (magnitude > EPSILON) {
        return map_scale(v, inverse);
    }
    return fallback;
}

V limit_angle(V target, V from, float maximum) {
    const V normalized_from = normalize_safe(from, V{});
    const V normalized_target = normalize_safe(target, V{});
    V axis = cross(normalized_target, normalized_from);
    const float cosine = vector_min(vector_max(dot3(normalized_target, normalized_from), -1.0f), 1.0f);
    const float angle = trigonometry::acos(cosine);
    // The limiter uses floor(angle/tau + .5), unlike its caller's fraction wrap.
    const float turns = rs::floor(rs::mul_add(angle, INV_TAU, 0.5f));
    const float closest_angle = rs::mul_add(-turns, TAU, angle);
    if (rs::abs(closest_angle) < maximum || dot3(axis, axis) < rs::from_bits(0x37800000u)) {
        return target;
    }
    axis = normalize(axis);
    const auto [sin, cos] = trigonometry::sin_cos(-maximum * 0.5f);
    const V q = map_scale(axis, sin);
    const V first = cross(q, from);
    V intermediate{};
    for (std::size_t i = 0; i < 4; ++i) intermediate[i] = rs::mul_add(cos, from[i], first[i]);
    const V second = cross(q, intermediate);
    V rotated{};
    for (std::size_t i = 0; i < 4; ++i) rotated[i] = rs::mul_add(second[i], 2.0f, from[i]);
    const float magnitude = length(target);
    return map_scale(rotated, magnitude);
}

V rotate_heading(V heading, V up, float angle) {
    const float x = up[0], y = up[1], z = up[2];
    const auto [s, c] = trigonometry::sin_cos(angle);
    const float t = 1.0f - c;
    const float tx = t * x, ty = t * y, tz = t * z;
    const float sx = s * x, sy = s * y, sz = s * z;
    const float xx = rs::mul_add(tx, x, c);
    const float yx = ty * x - sz;
    const float zx = rs::mul_add(tz, x, sy);
    const std::array<V, 3> columns{{
        {xx, rs::mul_add(tx, y, sz), tx * z - sy, xx},
        {yx, rs::mul_add(ty, y, c), rs::mul_add(ty, z, sx), yx},
        {zx, tz * y - sx, rs::mul_add(tz, z, c), zx},
    }};
    V out{};
    for (std::size_t i = 0; i < 4; ++i) {
        const float first = columns[0][i] * heading[0];
        const float second = rs::mul_add(columns[1][i], heading[1], first);
        out[i] = rs::mul_add(columns[2][i], heading[2], second);
    }
    return out;
}

}  // namespace s3::air::reckoning::math
