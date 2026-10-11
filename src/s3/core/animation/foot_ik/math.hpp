// Port of skate-core/src/animation/foot_ik/math.rs (whole file).
// Matrix interpolation used by SkeletonIK, TU3 82BD3150/82BD2E60.
// The shared Xenon estimate primitives remain host approximations pending
// hardware comparison; the recovered branch and refinement order is retained.
#pragma once
#include <array>
#include <cstddef>
#include <utility>

#include "s3/core/physics/skeleton_animation_record.hpp"

namespace s3::animation::foot_ik::math {

using Transform = physics::skeleton_animation_record::AnimationPartTransform;
using Vector = std::array<float, 4>;

// pub(super) helpers.
float reciprocal(float value, std::size_t refinements);
float length(Vector value);
Vector normalize(Vector value);
Vector cross(Vector left, Vector right);
/// 82BD3D90. The very short-vector branch preserves the original fourth lane.
Vector limit_length(Vector value, float limit);
/// 82BD2E60's axis/angle outputs. The unused screw-pivot output is omitted.
std::pair<Vector, float> rotation_axis_angle(const Transform& rotation);
/// Rodrigues construction in82BD34E0..3548. The native permutation duplicates
/// each column's X component into W; W is not a fabricated affine zero.
Transform axis_rotation(Vector axis, float angle);

/// 82BD3150 returns the remaining rotation angle as well as the blended frame.
std::pair<Transform, float> interpolate(const Transform& a, const Transform& b, float weight);
/// 82E0A570 overrides the interpolation helper's translation, including at
/// endpoint weights. Preserve its weighted-sum rounding instead of delta lerp.
Transform interpolate_affine(const Transform& a, const Transform& b, float weight);
/// General inverse expanded at8296FB50..FD14 and SkeletonData Init82BD6E9C.
/// The transpose permutation retains the second cofactor in the fourth lane.
Transform inverse_affine(const Transform& frame);

}  // namespace s3::animation::foot_ik::math
