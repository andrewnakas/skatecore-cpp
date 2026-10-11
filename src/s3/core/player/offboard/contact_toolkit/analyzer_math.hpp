// Port of skate-core/src/player/offboard/contact_toolkit/analyzer_math.rs (whole file).
// Geometric helpers82E0A060/82E0A258/82D85C48 and tangent8252D980.
#pragma once
#include <optional>

#include "s3/core/player/offboard/contact_toolkit/base.hpp"

namespace s3::player::offboard::contact_toolkit::analyzer_math {

inline Vector madd(Vector a, float s, Vector b) {
    return rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(a[i], s, b[i]); });
}
inline float reciprocal(float x) {
    float r = physics::native_arithmetic::reciprocal_estimate(x);
    for (int i = 0; i < 2; ++i) r = rs::mul_add(r, rs::mul_add(-r, x, 1.0f), r);
    return r;
}
inline Vector normalize(Vector v, Vector fallback) {
    const float l = length(v);
    return l > rs::from_bits(0x358637bdu) ? scale(v, reciprocal(l)) : fallback;
}
std::optional<Vector> plane_segment(Vector p, Vector n, Vector a, Vector b);
Vector clamp_normal(Vector a, Vector b, Vector n);
float intersection(std::array<float, 2> a, std::array<float, 2> b, std::array<float, 2> c, std::array<float, 2> d);
float tangent(float x);

}  // namespace s3::player::offboard::contact_toolkit::analyzer_math
