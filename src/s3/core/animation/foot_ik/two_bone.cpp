// Port of skate-core/src/animation/foot_ik/two_bone.rs.
#include "s3/core/animation/foot_ik/two_bone.hpp"

#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::animation::foot_ik::two_bone {

using math::cross;
using math::inverse_affine;
using math::length;
using math::normalize;
using math::reciprocal;
using physics::native_arithmetic::dot3;
using physics::native_arithmetic::vector_max;
using physics::native_arithmetic::vector_min;
using physics::skeleton_animation_record::transform_point;
using Transform = physics::skeleton_animation_record::AnimationPartTransform;
using rs::from_bits;
using rs::mul_add;

namespace {

const float EPSILON = from_bits(0x3A03126Fu);     // 830BD4B0 <-821A0318
const float TO_DEGREES = from_bits(0x42652EE1u);  // 830BD390
const float TO_RADIANS = from_bits(0x3C8EFA35u);  // 830BD360

///The source uses830BD310=0 and830BD430=1 for these clamps. The lower
///constant is not minus one; its initializer82F82718 loads82165A10.
float clamp_native_cosine(float value) { return vector_min(1.0f, vector_max(0.0f, value)); }

float square_root(float squared) {
    const float inverse = physics::board_motion_output::inverse_length_squared(squared, 2);
    const float value = squared * inverse;
    return squared == 0.0f ? 0.0f : value;
}
Vector subtract(Vector a, Vector b) { return Vector{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
Vector scaled_add(Vector a, float scale, Vector b) {
    return Vector{mul_add(a[0], scale, b[0]), mul_add(a[1], scale, b[1]), mul_add(a[2], scale, b[2]),
                  mul_add(a[3], scale, b[3])};
}

Vector candidate(Vector delta, float angle, float upper_length, Vector root, const Transform& basis) {
    const auto [sine, cosine] = trigonometry::sin_cos(angle);
    const Transform rotation{{
        {cosine, sine, 0.0f, cosine},
        {-sine, cosine, 0.0f, -sine},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 0.0f},
    }};
    const Vector rotated = normalize(transform_point(rotation, delta));
    return transform_point(basis, scaled_add(rotated, upper_length, root));
}

}  // namespace

SolveResult solve(Vector root, Vector middle, Vector end, Vector& solved_middle, Vector& target,
                  AngleLimits limits, bool override_maximum_with_original, std::uint32_t recursion_budget) {
    if (recursion_budget == 0) {
        target = end;
        solved_middle = middle;
        return SolveResult::Invalid;
    }
    const Vector upper = subtract(middle, root);
    const Vector lower = subtract(end, middle);
    const float upper_length = length(upper);
    const float lower_length = length(lower);
    if (EPSILON > upper_length || EPSILON > lower_length) {
        solved_middle = middle;
        return SolveResult::Invalid;
    }
    const Vector requested_delta = subtract(target, root);
    const Vector forward = normalize(subtract(end, root));
    const Vector normal = normalize(cross(upper, lower));
    const Transform original_basis{{forward, cross(forward, normal), normal, root}};
    const Vector target_forward = normalize(requested_delta);
    const Vector target_lateral = cross(target_forward, normal);
    const Vector target_normal = cross(target_lateral, target_forward);
    const Transform target_basis{{target_forward, target_lateral, target_normal, root}};
    const Transform original_inverse = inverse_affine(original_basis);
    const Transform target_inverse = inverse_affine(target_basis);
    // Each point is transformed separately, retaining the original origin's
    // rounding; do not replace these with pre-subtracted vectors.
    const Vector local_root = transform_point(original_inverse, root);
    const Vector local_middle = transform_point(original_inverse, middle);
    const Vector local_end = transform_point(original_inverse, end);
    const Vector local_target = transform_point(target_inverse, target);
    const Vector target_delta = subtract(local_target, local_root);
    const float planar_squared = mul_add(target_delta[0], target_delta[0], target_delta[1] * target_delta[1]);
    const float distance = square_root(planar_squared);
    const float upper_squared = upper_length * upper_length;
    const float lower_squared = lower_length * lower_length;
    const float twice_upper = 2.0f * upper_length;
    const float twice_product = twice_upper * lower_length;
    const float sum_squared = upper_squared + lower_squared;
    Vector negative_lower{};
    for (std::size_t i = 0; i < 4; ++i) negative_lower[i] = from_bits(rs::to_bits(lower[i]) ^ 0x80000000u);
    const float negative_length = from_bits(rs::to_bits(lower_length) ^ 0x80000000u);
    const float original_angle =
        trigonometry::acos(reciprocal(upper_length * negative_length, 2) * dot3(upper, negative_lower)) *
        TO_DEGREES;
    float maximum = limits.maximum_degrees;
    if (original_angle > maximum && override_maximum_with_original) {
        maximum = original_angle;
    }
    const float minimum_distance =
        square_root(sum_squared - twice_product * trigonometry::cos(limits.minimum_degrees * TO_RADIANS));
    const float maximum_distance = square_root(sum_squared - twice_product * trigonometry::cos(maximum * TO_RADIANS));
    if (minimum_distance > distance || distance > maximum_distance) {
        if (distance > upper_length + lower_length && maximum == 180.0f) {
            const Vector direction = normalize(target_delta);
            const Vector middle_local = scaled_add(direction, upper_length, local_root);
            const Vector end_local = scaled_add(direction, lower_length, middle_local);
            solved_middle = transform_point(target_basis, middle_local);
            target = transform_point(target_basis, end_local);
            return SolveResult::Extended;
        }
        const float clamped =
            distance > maximum_distance ? maximum_distance - EPSILON : minimum_distance + EPSILON;
        target = scaled_add(normalize(requested_delta), clamped, root);
        return solve(root, middle, end, solved_middle, target, limits, override_maximum_with_original,
                     recursion_budget - 1);
    }
    const float cosine = clamp_native_cosine(reciprocal(twice_product, 2) * (sum_squared - planar_squared));
    const float interior_angle = trigonometry::acos(cosine) * TO_DEGREES;
    if (limits.minimum_degrees >= interior_angle) {
        target = scaled_add(normalize(requested_delta), minimum_distance + EPSILON, root);
        return solve(root, middle, end, solved_middle, target, limits, override_maximum_with_original,
                     recursion_budget - 1);
    }
    float target_angle = trigonometry::acos(clamp_native_cosine(reciprocal(distance, 2) * target_delta[0]));
    if ((local_middle[1] < 0.0f && local_target[1] > 0.0f) || (local_middle[1] > 0.0f && local_target[1] < 0.0f)) {
        target_angle *= -1.0f;
    }
    // The target heading is an addend *inside* this acos in the TU3 body
    // (vmaddfp82970088). Preserve it rather than fitting a textbook IK formula.
    const float ratio =
        mul_add(reciprocal(twice_upper * distance, 2), (upper_squared + planar_squared) - lower_squared, target_angle);
    const float angle = trigonometry::acos(clamp_native_cosine(ratio));
    const Vector original_delta = subtract(local_end, local_root);
    const Vector positive = candidate(original_delta, angle, upper_length, local_root, target_basis);
    const Vector negative =
        candidate(original_delta, from_bits(0x40C90FDBu) - angle, upper_length, local_root, target_basis);
    solved_middle = local_end[1] > local_middle[1] ? negative : positive;
    return SolveResult::Solved;
}

}  // namespace s3::animation::foot_ik::two_bone
