// Port of skate-core/src/riding/reckoning_frames.rs (whole file).
// Reckoning transform, dynamic lean and lateral tilt, TU3
// 82D8D688/82D8D930/82D8C4F8. State scheduling and frame producers are separate.
// Numerical primitives retain their documented hardware-validation boundary.
#pragma once
#include <array>

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::riding::reckoning_frames {

using physics::skeleton_animation_record::AnimationPartTransform;
using physics::skeleton_animation_record::IDENTITY;
using point_graph::PointGraph;

struct ReckoningFrames {
    /// 752: ground frame, whose translation is retained by CalculateTransform.
    AnimationPartTransform ground = IDENTITY;
    /// 816: system frame after application of the body-flip transform.
    AnimationPartTransform system = IDENTITY;
    /// 880: same frame before body flip, used by CalculateTilt.
    AnimationPartTransform unflipped = IDENTITY;
    /// 944: rigid inverse of the final system frame.
    AnimationPartTransform inverse_system = IDENTITY;
    /// 1008: body-flip transform, produced by UpdateBodyFlip.
    AnimationPartTransform body_flip = IDENTITY;
    /// 1200: heading, reprojected onto the current up plane on each calculation.
    std::array<float, 4> heading{1.0f, 0.0f, 0.0f, 0.0f};
    /// 1576, published to PhysOutSystemReckoning160 with the stance sign.
    float target_lean_angle = 0.0f;
    /// 1248: first lane consumed by the body-tilt animation handler.
    std::array<float, 4> lateral_tilt{};

    /// 82D33178 initializes each matrix independently to identity and seeds
    /// heading to world X. Reset82D8C3A8 later preserves the matrices.
    static ReckoningFrames new_() { return ReckoningFrames{}; }

    /// 82D8D688. Cross products use the unnormalized intermediate axis;
    /// normalization has two refinements and no epsilon fallback in this leaf.
    void calculate_transform(std::array<float, 4> up, std::array<float, 4> ground_normal);
    /// 82D8D930. Signed angle is wrapped by fraction/floor, not scalar atan2.
    void calculate_dynamic_lean(std::array<float, 4> up, std::array<float, 4> dynamic_up);
    /// 82D8C4F8. Degenerate projection preserves the preceding lateral tilt.
    /// The stock source uses the unflipped up/forward axes and two authored
    /// curves, including its absolute wrapped angle and stance sign change.
    void calculate_tilt(bool reverse_stance, const PointGraph<8>& tilt_vs_angle,
                        const PointGraph<8>& tilt_vs_up);
};

// Module-private in Rust; declared here because the ported tests call them.
std::array<float, 4> normalize(std::array<float, 4> value);
/// [`normalize`], but a degenerate vector keeps `fallback` instead of becoming
/// NaN. The squared length is tested against zero *after* the f32 multiply, so
/// this also catches a vector whose components are small enough that `dot3`
/// underflows to exact zero while the vector itself is not zero.
std::array<float, 4> normalize_or(std::array<float, 4> value, std::array<float, 4> fallback);

}  // namespace s3::riding::reckoning_frames
