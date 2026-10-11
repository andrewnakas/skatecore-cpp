// Port of skate-core/src/player/offboard/ground_query/math.rs (whole file).
#pragma once
#include "s3/core/math.hpp"
#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::player::offboard::ground_query::math {

using s3::math::Vector3;

inline Vector3 add(Vector3 a, Vector3 b) { return Vector3::new_(a.x + b.x, a.y + b.y, a.z + b.z); }
inline Vector3 sub(Vector3 a, Vector3 b) { return Vector3::new_(a.x - b.x, a.y - b.y, a.z - b.z); }
inline Vector3 scale(Vector3 a, float k) { return Vector3::new_(a.x * k, a.y * k, a.z * k); }
inline Vector3 madd(Vector3 a, float k, Vector3 b) {
    return Vector3::new_(rs::mul_add(a.x, k, b.x), rs::mul_add(a.y, k, b.y), rs::mul_add(a.z, k, b.z));
}
inline float dot(Vector3 a, Vector3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vector3 cross(Vector3 a, Vector3 b) {
    return Vector3::new_(rs::mul_add(-a.z, b.y, a.y * b.z), rs::mul_add(-a.x, b.z, a.z * b.x),
                         rs::mul_add(-a.y, b.x, a.x * b.y));
}
inline Vector3 horizontal(Vector3 a) { return Vector3::new_(a.x, 0.0f, a.z); }
inline Vector3 abs(Vector3 a) { return Vector3::new_(rs::abs(a.x), rs::abs(a.y), rs::abs(a.z)); }
// Independent PC estimate, retaining the two TU3 refinement steps; not bit-exact Xenon.
inline float inverse_root(float q) {
    float r = physics::native_arithmetic::reciprocal_square_root_estimate(q);
    for (int i = 0; i < 2; ++i) r = rs::mul_add(r * 0.5f, rs::mul_add(-q, r * r, 1.0f), r);
    return r;
}
inline float length(Vector3 a) {
    const float q = dot(a, a);
    const float l = q * inverse_root(q);
    return q == 0.0f ? 0.0f : l;
}
inline float reciprocal(float x) {
    float r = physics::native_arithmetic::reciprocal_estimate(x);
    for (int i = 0; i < 2; ++i) r = rs::mul_add(r, rs::mul_add(-x, r, 1.0f), r);
    return r;
}
inline Vector3 safe_unit(Vector3 a, Vector3 fallback) {
    const float q = dot(a, a);
    const float r = inverse_root(q);
    const float l = q == 0.0f ? 0.0f : q * r;
    return l > rs::from_bits(0x358637bdu) ? scale(a, r) : fallback;
}

}  // namespace s3::player::offboard::ground_query::math
