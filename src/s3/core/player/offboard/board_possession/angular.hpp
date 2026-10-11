// Port of skate-core/src/player/offboard/board_possession/angular.rs (whole file).
// 82C07328: requested angular displacement becomes deck angular acceleration.
// Keep both inverse-inertia transforms: cancellation would change rounding and
// singular-matrix behavior.
#pragma once
#include "s3/core/player/offboard/board_possession/math.hpp"

namespace s3::player::offboard::board_possession::angular {

using math::Vector;

inline Vector acceleration_delta(Vector request, Vector omega, std::array<Vector, 3> inverse_inertia) {
    using namespace math;
    const Vector a = inverse_inertia[0], b = inverse_inertia[1], c = inverse_inertia[2];
    const std::array<Vector, 3> cofactor{cross(b, c), cross(c, a), cross(a, b)};
    const float determinant = dot(a, cofactor[0]);
    const float inv_det = reciprocal(determinant, 2);
    std::array<Vector, 3> inverse{};
    for (std::size_t i = 0; i < 3; ++i) {
        inverse[i] = Vector{cofactor[0][i] * inv_det, cofactor[1][i] * inv_det, cofactor[2][i] * inv_det, 0.0f};
    }
    const float dt = rs::from_bits(0x3c888889u);
    const Vector axis = unit(request, Vector{});
    const Vector residual = sub(request, scale(axis, dot(axis, scale(omega, dt))));
    const Vector local =
        transform_direction(Frame{inverse[0], inverse[1], inverse[2], Vector{}}, scale(residual, reciprocal(dt, 2)));
    return transform_direction(Frame{a, b, c, Vector{}}, scale(local, reciprocal(dt, 2)));
}

}  // namespace s3::player::offboard::board_possession::angular
