// Port of skate-core/src/player/offboard/grab_scene/math.rs (whole file).
#pragma once
#include "s3/core/player/offboard/grab_scene.hpp"

namespace s3::player::offboard::grab_scene::math {

inline float dot(Vector a, Vector b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
inline Vector sub(Vector a, Vector b) { return Vector{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
inline Vector madd(Vector a, float k, Vector b) {
    return rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(a[i], k, b[i]); });
}
inline float length(Vector v) {
    const float d = dot(v, v);
    float r = rs::recip(rs::sqrt(d));
    for (int i = 0; i < 2; ++i) r = rs::mul_add(r * 0.5f, rs::mul_add(-d, r * r, 1.0f), r);
    return d == 0.0f ? 0.0f : d * r;
}
inline float reciprocal(float v) {
    float r = rs::recip(v);
    for (int i = 0; i < 2; ++i) r = rs::mul_add(r, rs::mul_add(-r, v, 1.0f), r);
    return r;
}
inline Vector point(const Frame& f, Vector v) { return madd(f[2], v[2], madd(f[1], v[1], madd(f[0], v[0], f[3]))); }
inline Vector direction(const Frame& f, Vector v) {
    return madd(f[2], v[2], madd(f[1], v[1], rs::map(f[0], [&](float x) { return x * v[0]; })));
}
/// 82ADD910 ->82ADDB48 positive-result branch, fatness zero.
inline bool sphere_segment(Vector center, float radius, Vector start, Vector end) {
    const Vector c = sub(center, start);
    const float rr = radius * radius;
    if (dot(c, c) < rr) return true;
    const Vector d = sub(end, start);
    const float projection = dot(c, d);
    if (!(projection > 0.0f)) return false;
    const float dd = dot(d, d);
    const Vector cross{rs::mul_add(-c[2], d[1], c[1] * d[2]), rs::mul_add(-c[0], d[2], c[2] * d[0]),
                       rs::mul_add(-c[1], d[0], c[0] * d[1]), 0.0f};
    const float discriminant = dd * rr - dot(cross, cross);
    const float beyond = projection - dd;
    return !(discriminant < 0.0f || (beyond > 0.0f && beyond * beyond > discriminant));
}

}  // namespace s3::player::offboard::grab_scene::math
