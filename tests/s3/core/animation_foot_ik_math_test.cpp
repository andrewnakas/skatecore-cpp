// Port of the math tests in skate-core/src/animation/foot_ik/tests.rs
// (inverse_preserves_points_for_scaled_and_sheared_authored_frames,
//  frame_interpolation_uses_rotation_and_translation_at_the_same_weight).
// The remaining foot_ik tests need status/contact/two_bone (animation stage).
#include "harness.hpp"
#include "s3/core/animation/foot_ik.hpp"

namespace {
using s3::animation::foot_ik::math::interpolate;
using s3::animation::foot_ik::math::inverse_affine;
using s3::animation::foot_ik::math::Transform;
using s3::physics::skeleton_animation_record::IDENTITY;
using s3::physics::skeleton_animation_record::transform_point;
namespace consts = s3::rs::f32::consts;

S3_TEST(inverse_preserves_points_for_scaled_and_sheared_authored_frames) {
    const Transform frame{{
        {2.0f, 0.0f, 0.0f, 0.0f},
        {0.5f, 3.0f, 0.0f, 0.0f},
        {0.0f, 0.25f, 4.0f, 0.0f},
        {7.0f, -2.0f, 5.0f, 0.0f},
    }};
    const std::array<float, 4> point{0.3f, -0.7f, 1.2f, 0.0f};
    const auto recovered = transform_point(inverse_affine(frame), transform_point(frame, point));
    for (std::size_t lane = 0; lane < 3; ++lane) {
        CHECK(s3::rs::abs(recovered[lane] - point[lane]) < 2e-6f);
    }
}

S3_TEST(frame_interpolation_uses_rotation_and_translation_at_the_same_weight) {
    Transform target = IDENTITY;
    target[0] = {0.0f, 1.0f, 0.0f, 0.0f};
    target[1] = {-1.0f, 0.0f, 0.0f, 0.0f};
    target[3] = {8.0f, 2.0f, -4.0f, 0.0f};
    const auto [half, remaining] = interpolate(IDENTITY, target, 0.5f);
    CHECK(s3::rs::abs(half[0][0] - consts::FRAC_1_SQRT_2) < 2e-6f);
    CHECK(s3::rs::abs(half[0][1] - consts::FRAC_1_SQRT_2) < 2e-6f);
    CHECK_EQ((std::array<float, 3>{half[3][0], half[3][1], half[3][2]}),
             (std::array<float, 3>{4.0f, 1.0f, -2.0f}));
    CHECK(s3::rs::abs(remaining - consts::FRAC_PI_4) < 2e-6f);
    const auto full = interpolate(IDENTITY, target, 1.0f);
    CHECK_EQ(full.first, target);
    CHECK_EQ(full.second, 0.0f);
}
}  // namespace
