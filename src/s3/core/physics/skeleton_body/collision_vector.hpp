// Port of skate-core/src/physics/skeleton_body/collision_vector.rs (included via skeleton_body.hpp).
// Original vector arithmetic used by82BD4A30 and82BD5A00. (pub(super) helpers.)
#pragma once
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/reciprocal_sqrt.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"

namespace s3::physics::skeleton_body::collision_vector {

using collision_feedback::V;
using skeleton_animation_record::AnimationPartTransform;

inline float dot(V a, V b) { return native_arithmetic::dot3(a, b); }
inline V scale(V v, float s) { return V{v[0] * s, v[1] * s, v[2] * s, v[3] * s}; }
inline V sub(V a, V b) { return V{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
inline V add(V a, V b) { return V{a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]}; }
inline float inverse_length(float square) {
    float inverse = reciprocal_sqrt::estimate(square);
    for (int i = 0; i < 2; ++i) {
        inverse = rs::mul_add(inverse * 0.5f, rs::mul_add(-square, inverse * inverse, 1.0f), inverse);
    }
    return inverse;
}
/// The same guard, and the same widening, as [`board_motion_output::length`].
///
/// This is the other half of the native `82BD3D90` pair, and it feeds the `clamp_length` below,
/// whose `size < ...` early return is likewise false against NaN. A ragdoll limb settling against
/// a surface reaches denormal squared speeds exactly as the up vector does.
inline float length(V v) {
    const float square = dot(v, v);
    if (square < rs::f32::MIN_POSITIVE) {
        return 0.0f;
    }
    return square * inverse_length(square);
}
inline V normalize(V v) {
    const float square = dot(v, v);
    const float inverse = inverse_length(square);
    const float size = square == 0.0f ? 0.0f : square * inverse;
    if (size > 1.0e-6f) {
        return scale(v, inverse);
    }
    return V{0.0f, 0.0f, 0.0f, 0.0f};
}
///82BD3D90 keeps all stored lanes, including the unused fourth velocity lane.
inline V clamp_length(V v, float maximum) {
    const float size = length(v);
    if (size < rs::from_bits(0x37800000u)) {
        return v;
    }
    const float bounded = maximum - size >= 0.0f ? size : maximum;
    float inverse = native_arithmetic::reciprocal_estimate(size);
    for (int i = 0; i < 2; ++i) {
        inverse = rs::mul_add(inverse, rs::mul_add(-inverse, size, 1.0f), inverse);
    }
    return scale(scale(v, bounded), inverse);
}
inline V transform(const AnimationPartTransform& m, V v) {
    V out{};
    for (std::size_t i = 0; i < 4; ++i) {
        out[i] = rs::mul_add(m[2][i], v[2], rs::mul_add(m[1][i], v[1], rs::mul_add(m[0][i], v[0], m[3][i])));
    }
    return out;
}
inline V cross(V a, V b) {
    return V{
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
        rs::mul_add(-a[3], b[3], a[3] * b[3]),
    };
}

}  // namespace s3::physics::skeleton_body::collision_vector
