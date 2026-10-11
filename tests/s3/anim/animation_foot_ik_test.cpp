// Port of skate-core/src/animation/foot_ik/tests.rs.
#include "harness.hpp"
#include "s3/core/animation/foot_ik.hpp"

using namespace s3::animation::foot_ik;
using contact::ContactInput;
using contact::ContactState;
using math::interpolate;
using math::inverse_affine;
using s3::physics::skeleton_animation_record::IDENTITY;
using s3::physics::skeleton_animation_record::transform_point;
using status::LimbStatus;
using status::Mode;
using transforms::LimbFrames;
using two_bone::AngleLimits;
using two_bone::SolveResult;
using Transform = s3::physics::skeleton_animation_record::AnimationPartTransform;
using Vec4 = std::array<float, 4>;

S3_TEST(inverse_preserves_points_for_scaled_and_sheared_authored_frames) {
    const Transform frame{{
        {2.0f, 0.0f, 0.0f, 0.0f},
        {0.5f, 3.0f, 0.0f, 0.0f},
        {0.0f, 0.25f, 4.0f, 0.0f},
        {7.0f, -2.0f, 5.0f, 0.0f},
    }};
    const Vec4 point = {0.3f, -0.7f, 1.2f, 0.0f};
    const Vec4 recovered = transform_point(inverse_affine(frame), transform_point(frame, point));
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
    CHECK(s3::rs::abs(half[0][0] - s3::rs::f32::consts::FRAC_1_SQRT_2) < 2e-6f);
    CHECK(s3::rs::abs(half[0][1] - s3::rs::f32::consts::FRAC_1_SQRT_2) < 2e-6f);
    CHECK_EQ(half[3][0], 4.0f);
    CHECK_EQ(half[3][1], 1.0f);
    CHECK_EQ(half[3][2], -2.0f);
    CHECK(s3::rs::abs(remaining - s3::rs::f32::consts::FRAC_PI_4) < 2e-6f);
    const auto full = interpolate(IDENTITY, target, 1.0f);
    CHECK(full.first == target);
    CHECK_EQ(full.second, 0.0f);
}

S3_TEST(reachable_two_bone_target_keeps_both_segment_lengths) {
    const Vec4 root = {0.0f, 0.0f, 0.0f, 0.0f};
    const Vec4 middle = {0.3f, 0.4f, 0.0f, 0.0f};
    const Vec4 end = {0.6f, 0.0f, 0.0f, 0.0f};
    Vec4 target = {0.7f, 0.0f, 0.0f, 0.0f};
    Vec4 solved = {0.0f, 0.0f, 0.0f, 0.0f};
    const SolveResult result = two_bone::solve(root, middle, end, solved, target, AngleLimits{5.0f, 175.0f}, true, 2);
    CHECK(result == SolveResult::Solved);
    CHECK_EQ(target[0], 0.7f);
    CHECK_EQ(target[1], 0.0f);
    CHECK_EQ(target[2], 0.0f);
    auto distance = [](Vec4 a, Vec4 b) {
        return s3::rs::sqrt(s3::rs::powi(a[0] - b[0], 2) + s3::rs::powi(a[1] - b[1], 2) +
                            s3::rs::powi(a[2] - b[2], 2));
    };
    CHECK(s3::rs::abs(distance(root, solved) - 0.5f) < 2e-5f);
    CHECK(s3::rs::abs(distance(solved, target) - 0.5f) < 2e-5f);
}

S3_TEST(foot_queries_affect_the_following_update_and_loss_sets_support_history) {
    ContactState contacts{};
    std::array<LimbStatus, 4> statuses{};
    statuses[0].mode = Mode::OnDeck;
    std::array<LimbFrames, 4> frames{};
    auto input = [](std::optional<Vec4> hit) {
        return ContactInput{0x02000000u, 10, {10, 20}, {0.0f, 0.0f, 0.0f, 0.0f}, IDENTITY, IDENTITY, {hit, std::nullopt}};
    };
    contacts.update(statuses, frames, input(std::nullopt));
    CHECK(!contacts.support_failed_this_update);
    CHECK_EQ(contacts.feet[0].query_state, 2u);
    contacts.update(statuses, frames, input(Vec4{0.0f, 0.0f, 0.0f, 0.0f}));
    CHECK(contacts.support_failed_this_update);
    CHECK(contacts.support_failed);
    CHECK_EQ(contacts.feet[0].query_state, 1u);
    statuses[0].mode = Mode::Disabled;
    contacts.update(statuses, frames, input(std::nullopt));
    CHECK(!contacts.support_failed_this_update);
    CHECK(contacts.support_failed);
}
