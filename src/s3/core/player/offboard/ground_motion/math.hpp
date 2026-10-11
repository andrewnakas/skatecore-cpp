// Port of skate-core/src/player/offboard/ground_motion/math.rs (whole file).
// Original VMX operation trees expressed with ordinary PC floating-point math.
// PC seeds and dot sums are not a bit-exact Xenon emulation claim.
#pragma once
#include "s3/core/physics/skeleton_root.hpp"
#include "s3/core/player/offboard/ground_motion.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::player::offboard::ground_motion::math {

inline constexpr Vector ZERO{0.0f, 0.0f, 0.0f, 0.0f};
inline constexpr Vector UP{0.0f, 1.0f, 0.0f, 0.0f};
inline constexpr Frame IDENTITY{{{1.0f, 0.0f, 0.0f, 0.0f}, UP, {0.0f, 0.0f, 1.0f, 0.0f}, ZERO}};
inline const float DT = rs::from_bits(0x3c888889u);
inline const float RATE = rs::from_bits(0x426fffffu);
inline const float EPSILON = rs::from_bits(0x3a83126fu);
inline const float NORMAL_EPSILON = rs::from_bits(0x358637bdu);

inline Vector add(Vector a, Vector b) { return Vector{a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]}; }
inline Vector sub(Vector a, Vector b) { return Vector{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
inline Vector scale(Vector a, float b) { return Vector{a[0] * b, a[1] * b, a[2] * b, a[3] * b}; }
inline Vector madd(Vector a, float b, Vector c) {
    return rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(a[i], b, c[i]); });
}
inline float dot(Vector a, Vector b) { return (a[0] * b[0] + a[1] * b[1]) + a[2] * b[2]; }
inline Vector cross(Vector a, Vector b) {
    return Vector{rs::mul_add(-a[2], b[1], a[1] * b[2]), rs::mul_add(-a[0], b[2], a[2] * b[0]),
                  rs::mul_add(-a[1], b[0], a[0] * b[1]), rs::mul_add(-a[3], b[3], a[3] * b[3])};
}
inline float inverse_sqrt(float q) {
    float r = 1.0f / rs::sqrt(q);
    for (int i = 0; i < 2; ++i) {
        const float square = r * r;
        const float half = r * 0.5f;
        r = rs::mul_add(half, rs::mul_add(-q, square, 1.0f), r);
    }
    return r;
}
inline float reciprocal(float d) {
    float r = 1.0f / d;
    for (int i = 0; i < 2; ++i) r = rs::mul_add(r, rs::mul_add(-d, r, 1.0f), r);
    return r;
}
inline float length(Vector v) {
    const float q = dot(v, v);
    const float x = q * inverse_sqrt(q);
    return q == 0.0f ? 0.0f : x;
}
inline Vector normalize(Vector v) { return scale(v, inverse_sqrt(dot(v, v))); }
inline Vector normalize_safe(Vector v, Vector fallback) {
    const float q = dot(v, v);
    const float r = inverse_sqrt(q);
    const Vector n = scale(v, r);
    const float len = q == 0.0f ? 0.0f : q * r;
    return len > NORMAL_EPSILON ? n : fallback;
}
inline float select(float test, float positive, float negative) { return test >= 0.0f ? positive : negative; }
inline float clamp_signed(float v) {
    const float low = select(-1.0f - v, -1.0f, v);
    return select(1.0f - low, low, 1.0f);
}
inline Vector transform_vector(Vector v, const Frame& f) { return madd(f[2], v[2], madd(f[1], v[1], scale(f[0], v[0]))); }
inline Vector transform_point(Vector v, const Frame& f) {
    return madd(f[2], v[2], madd(f[1], v[1], madd(f[0], v[0], f[3])));
}
inline Frame compose(const Frame& a, const Frame& b) {
    return Frame{transform_vector(a[0], b), transform_vector(a[1], b), transform_vector(a[2], b), transform_point(a[3], b)};
}
inline Frame inverse_rigid(const Frame& f) { return physics::skeleton_root::inverse_rigid(f); }

/// 82D7FEE0..FFCC XYZ Rodrigues rotation, using the host's geometric W=0
/// representation rather than native permutation scratch lanes.
inline Vector rotate(Vector v, Vector axis, float angle) {
    const float x = axis[0], y = axis[1], z = axis[2];
    const auto [s, c] = trigonometry::sin_cos(angle);
    const float t = 1.0f - c;
    const float tx = t * x, ty = t * y, tz = t * z;
    const float sx = s * x, sy = s * y, sz = s * z;
    const float xx = rs::mul_add(tx, x, c);
    const float yx = ty * x - sz;
    const float zx = rs::mul_add(tz, x, sy);
    const Vector c0{xx, rs::mul_add(tx, y, sz), tx * z - sy, 0.0f};
    const Vector c1{yx, rs::mul_add(ty, y, c), rs::mul_add(ty, z, sx), 0.0f};
    const Vector c2{zx, tz * y - sx, rs::mul_add(tz, z, c), 0.0f};
    return transform_vector(v, Frame{c0, c1, c2, ZERO});
}

/// Original82BD3E78, independently checked against its full raw body.
inline Vector limit_angle(Vector target, Vector from, float maximum) {
    const Vector a = normalize_safe(target, ZERO);
    const Vector b = normalize_safe(from, ZERO);
    Vector axis = cross(a, b);
    const float cosine = rs::min(rs::max(dot(a, b), -1.0f), 1.0f);
    const float angle = trigonometry::acos(cosine);
    const float tau = rs::from_bits(0x40c90fdbu);
    const float turns = rs::floor(rs::mul_add(angle, rs::from_bits(0x3e22f983u), 0.5f));
    const float closest = rs::mul_add(-turns, tau, angle);
    if (rs::abs(closest) < maximum || dot(axis, axis) < rs::from_bits(0x37800000u)) return target;
    axis = normalize(axis);
    const auto [s, c] = trigonometry::sin_cos(-maximum * 0.5f);
    Vector q = scale(axis, s);
    q[3] = c;  // 82BD4130 vrlimi inserts cosine in the quaternion W lane.
    const Vector first = cross(q, from);
    const Vector intermediate = madd(from, c, first);
    const Vector second = cross(q, intermediate);
    return scale(madd(second, 2.0f, from), length(target));
}

}  // namespace s3::player::offboard::ground_motion::math
