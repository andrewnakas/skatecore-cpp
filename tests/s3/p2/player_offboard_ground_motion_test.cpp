// Port of skate-core/src/player/offboard/ground_motion/tests.rs.
#include <cmath>

#include "harness.hpp"
#include "s3/core/player/offboard/ground_motion.hpp"
#include "s3/core/player/offboard/ground_motion/math.hpp"

namespace {
using namespace s3::player::offboard::ground_motion;
namespace math = s3::player::offboard::ground_motion::math;
using math::DT;
using math::IDENTITY;
using math::UP;
using math::ZERO;

GroundMotionState state() {
    GroundMotionState s;
    s.frame_0 = IDENTITY;
    s.published_frame_64 = IDENTITY;
    s.previous_support_frame_192 = IDENTITY;
    s.support_id_352 = 1;
    s.target_frame_608 = IDENTITY;
    s.target_scale_784 = -1.0f;
    return s;
}
GroundMotionInput input() {
    GroundMotionInput i;
    i.contact_frame_32 = IDENTITY;
    i.contact_normal_112 = UP;
    i.contact_id_180 = 1;
    i.requested_duration_288 = 1.0f;
    i.target_frame_368 = IDENTITY;
    i.reference_frame_128 = IDENTITY;
    i.desired_up_544 = UP;
    return i;
}
void near(float a, float b) { CHECK_MSG(std::fabs(a - b) < 1.0e-5f, a << " != " << b); }

S3_TEST(free_step_combines_all_actual_displacements) {
    auto s = state();
    auto i = input();
    s.frame_0[3] = {10.0f, 2.0f, 3.0f, 0.0f};
    s.velocity_480 = {6.0f, 0.0f, 0.0f, 0.0f};
    s.predicted_support_velocity_272 = {0.0f, 0.0f, 3.0f, 0.0f};
    i.velocity_addition_528 = {0.0f, 12.0f, 0.0f, 0.0f};
    i.contact_displacement_384 = {0.5f, 0.0f, 0.0f, 0.0f};
    update(s, i);
    near(s.frame_0[3][0], 10.6f);
    near(s.frame_0[3][1], 2.2f);
    near(s.frame_0[3][2], 3.05f);
    CHECK_EQ(s.frame_0, s.published_frame_64);
}

S3_TEST(absent_contact_preserves_support_history) {
    auto s = state();
    const auto i = input();
    s.support_velocity_256 = {1.0f, 1.0f, 1.0f, 1.0f};
    s.support_id_352 = 9;
    support::update(s, i);
    CHECK_EQ(s.support_velocity_256, (Vector{1.0f, 1.0f, 1.0f, 1.0f}));
    CHECK_EQ(s.support_id_352, 9u);
}

S3_TEST(new_support_restores_removed_velocity_and_resets_derivatives) {
    auto s = state();
    auto i = input();
    i.contact_flags_176 = 1;
    i.contact_id_180 = 2;
    s.support_velocity_removed_715 = true;
    s.predicted_support_velocity_272 = {3.0f, 0.0f, 0.0f, 0.0f};
    s.velocity_480 = {1.0f, 0.0f, 0.0f, 0.0f};
    s.predicted_support_yaw_344 = 5.0f;
    support::update(s, i);
    CHECK_EQ(s.support_id_352, 2u);
    near(s.velocity_480[0], 4.0f);
    near(s.speed_704, 4.0f);
    CHECK(!s.support_velocity_removed_715);
    CHECK_EQ(s.predicted_support_velocity_272, ZERO);
    CHECK_EQ(s.predicted_support_yaw_344, 0.0f);
}

S3_TEST(translated_support_predicts_velocity_and_removes_it_once) {
    auto s = state();
    auto i = input();
    i.contact_flags_176 = 1;
    i.contact_frame_32[3][0] = DT;
    s.velocity_480 = {10.0f, 0.0f, 0.0f, 0.0f};
    support::update(s, i);
    near(s.support_velocity_256[0], 1.0f);
    near(s.predicted_support_velocity_272[0], 2.0f);
    near(s.velocity_480[0], 8.0f);
    CHECK(s.support_velocity_removed_715);
    i.contact_frame_32[3][0] = 2.0f * DT;
    support::update(s, i);
    near(s.predicted_support_velocity_272[0], 1.0f);
    near(s.velocity_480[0], 8.0f);
}

S3_TEST(rotated_support_produces_signed_yaw_history) {
    auto s = state();
    auto i = input();
    i.contact_flags_176 = 1;
    const float angle = 0.1f;
    const float sin = std::sin(angle), cos = std::cos(angle);
    i.contact_frame_32[0] = {cos, 0.0f, -sin, 0.0f};
    i.contact_frame_32[2] = {sin, 0.0f, cos, 0.0f};
    support::update(s, i);
    near(s.support_yaw_336, angle * math::RATE);
    near(s.predicted_support_yaw_344, 2.0f * s.support_yaw_336);
}

S3_TEST(mirrored_support_acceleration_reverses_x_and_z_filter) {
    auto a = state();
    auto b = state();
    auto i = input();
    i.contact_flags_176 = 1;
    i.contact_frame_32[3] = {DT, 0.0f, DT, 0.0f};
    support::update(a, i);
    i.mirrored_292 = true;
    support::update(b, i);
    near(a.filtered_local_acceleration_320[0], -b.filtered_local_acceleration_320[0]);
    near(a.filtered_local_acceleration_320[2], -b.filtered_local_acceleration_320[2]);
    CHECK_EQ(a.filtered_local_acceleration_320[1], 0.0f);
}

S3_TEST(stationary_animation_reaches_target_at_requested_duration) {
    auto s = state();
    auto i = input();
    i.animation_directed_304 = true;
    i.requested_duration_288 = 2.0f;
    s.target_frame_608[3] = {0.0f, 0.0f, 10.0f, 0.0f};
    const auto [step, h] = approach::calculate(s, i, ZERO, IDENTITY);
    (void)h;
    near(s.target_scale_784, 5.0f);
    near(step[2], 5.0f * DT);
}

S3_TEST(moving_animation_uses_current_motion_length_and_retains_scale) {
    auto s = state();
    auto i = input();
    i.animation_directed_304 = true;
    i.animation_motion_224 = {0.0f, 0.0f, 2.0f, 0.0f};
    i.animation_motion_240 = {0.0f, 0.0f, 3.0f, 0.0f};
    s.target_frame_608[3] = {0.0f, 0.0f, 10.0f, 0.0f};
    const auto [step, h] = approach::calculate(s, i, ZERO, IDENTITY);
    (void)h;
    near(s.target_scale_784, 5.0f);
    near(step[2], 15.0f * DT);
    i.animation_motion_224[2] = 4.0f;
    approach::calculate(s, i, ZERO, IDENTITY);
    near(s.target_scale_784, 5.0f);
}

S3_TEST(target_refresh_uses_right_offset_and_applies_support_delta_after_step) {
    auto s = state();
    auto i = input();
    i.animation_directed_304 = true;
    i.target_frame_present_352 = true;
    i.target_frame_368[3] = {0.0f, 0.0f, 3.0f, 0.0f};
    Frame delta = IDENTITY;
    delta[3] = {2.0f, 0.0f, 0.0f, 0.0f};
    approach::calculate(s, i, ZERO, delta);
    near(s.target_frame_608[3][0], 2.2f);
    near(s.target_frame_608[3][2], 3.0f);
}

S3_TEST(contact_height_adjustment_is_five_percent_along_current_up) {
    auto s = state();
    auto i = input();
    i.contact_flags_176 = 1;
    i.contact_position_0 = {0.0f, 2.0f, 0.0f, 0.0f};
    const auto [step, height] = approach::calculate(s, i, ZERO, IDENTITY);
    (void)step;
    near(height[1], 0.1f);
}

S3_TEST(obstacle_approach_caps_step_to_velocity_budget) {
    auto s = state();
    auto i = input();
    s.velocity_480 = {0.0f, 0.0f, 6.0f, 0.0f};
    i.obstacle_enabled_713 = true;
    i.obstacle_target_672 = {0.0f, 0.0f, 10.0f, 0.0f};
    near(approach::calculate(s, i, ZERO, IDENTITY).first[2], 0.1f);
}

S3_TEST(nearby_target_uses_remaining_tangent_budget) {
    auto s = state();
    auto i = input();
    s.velocity_480 = {0.0f, 0.0f, 6.0f, 0.0f};
    i.obstacle_enabled_713 = true;
    i.obstacle_target_672 = {0.0f, 0.0f, 0.05f, 0.0f};
    near(approach::calculate(s, i, ZERO, IDENTITY).first[2], 0.1f);
}

S3_TEST(contact_target_lateral_component_is_removed) {
    auto s = state();
    auto i = input();
    s.velocity_480 = {0.0f, 0.0f, 6.0f, 0.0f};
    i.contact_flags_176 = 2;
    i.contact_target_96 = {9.0f, 0.0f, 1.0f, 0.0f};
    const auto step = approach::calculate(s, i, ZERO, IDENTITY).first;
    near(step[0], 0.0f);
    near(step[2], 0.1f);
}

S3_TEST(publication_decays_correction_without_moving_physical_frame) {
    auto s = state();
    s.correction_enabled_711 = true;
    s.correction_576 = {1.0f, 0.0f, 0.0f, 0.0f};
    publication::update(s, ZERO);
    near(s.correction_576[0], 0.95f);
    near(s.published_frame_64[3][0], -0.95f);
    near(s.frame_0[3][0], 0.0f);
    CHECK(s.correction_enabled_711);
}

S3_TEST(publication_threshold_and_large_opposing_step_clear_correction) {
    auto s = state();
    s.correction_enabled_711 = true;
    s.correction_576 = {0.01f, 0.0f, 0.0f, 0.0f};
    publication::update(s, ZERO);
    CHECK(!s.correction_enabled_711);
    s.correction_enabled_711 = true;
    s.correction_576 = {1.0f, 0.0f, 0.0f, 0.0f};
    publication::update(s, {-2.0f, 0.0f, 0.0f, 0.0f});
    CHECK_EQ(s.correction_576, ZERO);
    CHECK(!s.correction_enabled_711);
}

S3_TEST(limiter_preserves_parallel_target_and_caps_oblique_normal) {
    CHECK_EQ(math::limit_angle(UP, UP, 0.5f), UP);
    const auto v = math::limit_angle({1.0f, 0.0f, 0.0f, 0.0f}, UP, s3::rs::f32::consts::FRAC_PI_4);
    near(v[0], s3::rs::f32::consts::FRAC_1_SQRT_2);
    near(v[1], s3::rs::f32::consts::FRAC_1_SQRT_2);
}

S3_TEST(limiter_keeps_quaternion_w_lane_fused_residuals) {
    const float limit = 0.73f;
    const Vector from{0.0f, 1.0f, 0.0f, 0.37f};
    const auto got = math::limit_angle({1.0f, 0.0f, 0.0f, 0.0f}, from, limit);
    const float c = s3::trigonometry::sin_cos(-limit * 0.5f).second;
    const float first_w = s3::rs::mul_add(-c, from[3], c * from[3]);
    const float middle_w = s3::rs::mul_add(from[3], c, first_w);
    const float second_w = s3::rs::mul_add(-c, middle_w, c * middle_w);
    const float expected = s3::rs::mul_add(second_w, 2.0f, from[3]);
    CHECK_EQ(s3::rs::to_bits(got[3]), s3::rs::to_bits(expected));
}
}  // namespace
