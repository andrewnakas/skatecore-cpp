// Port of skate-core/src/physics/grind_forces.rs.
// TU3 grind force leaves. Their inputs belong to the retained grind manager;
// contact admission and lifecycle are separate from these force calculations.
//
// #[path] submodules (namespaces s3::physics::grind_forces::<name>, files grind_forces/<name>):
// launch, noise, orientation, post, reckoning, release, slide, support. Include them directly.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/core/rs.hpp"

namespace s3::physics::grind_forces {

using native_arithmetic::dot3;
using V = std::array<float, 4>;

/// 82D3FA18, called by the 50-50 update82D41D70 with800,0,.07.
/// Returns a force at the deck origin; None means no accumulator call.
std::optional<V> lateral_pin(std::array<V, 4> board, V point, V across, V velocity, float strength,
                             float forward_offset, float up_offset, bool forward_selected,
                             float slope_multiplier);

/// 82D3FD88 after its native surface/material selection. It damps velocity in
/// the plane perpendicular to the grind normal, including the along-rail lane.
V friction(V velocity, V grind_normal, V support, float time_multiplier, bool flagged_surface,
           float surface_multiplier, std::uint32_t geometry_kind, std::array<float, 3> strengths);

/// 82D3FD88 skips the accumulator call at or below its planar-speed threshold.
bool friction_applies(V velocity, V normal);

/// 82D3FD88's material switch. S3 adds material13 to S2's material2 case.
float material_multiplier(std::uint32_t material);

/// 82D3FA18: gravity relief bypasses the authored PinVsSlope four-point graph.
float pin_slope(V normal, V upmost_normal, float gravity_relief,
                const point_graph::PointGraph<4>& pin_vs_slope);

// ---- private helpers (visible to the submodules via `use super::...`) ----
inline V sub(const V& a, const V& b) { return V{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
inline V scale(const V& a, float scale_) {
    return V{a[0] * scale_, a[1] * scale_, a[2] * scale_, a[3] * scale_};
}
inline float length(const V& v) {
    const float square = dot3(v, v);
    if (square == 0.0f) {
        return 0.0f;
    }
    return square * board_motion_output::inverse_length_squared(square, 2);
}
inline float reciprocal(float value) {
    float inverse = rs::recip(value);
    for (int i = 0; i < 2; ++i) {
        const float correction = rs::mul_add(-inverse, value, 1.0f);
        inverse = rs::mul_add(inverse, correction, inverse);
    }
    return inverse;
}

}  // namespace s3::physics::grind_forces
