// Port of skate-core/src/player/offboard/ground_input/tests.rs.
#include <cmath>

#include "harness.hpp"
#include "s3/core/player/offboard/ground_input.hpp"
#include "s3/core/player/offboard/ground_input/math.hpp"

namespace {
using namespace s3::player::offboard::ground_input;
namespace math = s3::player::offboard::ground_input::math;
using s3::point_graph::PointGraph;
const float NAN_ = s3::rs::f32::NAN_;

GroundInput input() { return GroundInput{0, 9.0f, -7.0f, 0.0f, 1.0f, 1.0f, 1.0f, {0.0f, 0.0f, 1.0f}}; }
PointGraph<8> constant(float value) {
    PointGraph<8> g;
    g.x = {0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.6f, 0.8f, 1.0f};
    g.y.fill(value);
    return g;
}
PointGraph<8> angle_curve() {
    PointGraph<8> g;
    g.x = {0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.6f, 0.8f, 1.0f};
    g.y = g.x;
    return g;
}
void near(float actual, float expected) {
    CHECK_MSG(std::fabs(actual - expected) < 2.0e-6f, actual << " != " << expected);
}

S3_TEST(direct_flag_bypasses_invalid_geometry_and_scaling) {
    auto i = input();
    i.processed_flags_2472 = 0x10000000;
    i.frame_forward_112 = {NAN_, NAN_, NAN_};
    i.processed_scale_2912 = NAN_;
    i.processed_stick_2692 = NAN_;
    const auto output = calculate(i, constant(NAN_), constant(NAN_));
    CHECK(output == (GroundInputOutput{9.0f, -7.0f, {0.0f, 0.0f, 0.0f, 0.0f}}));
}

S3_TEST(other_flag_does_not_select_direct_input) {
    auto i = input();
    i.processed_flags_2472 = 0x08000000;
    near(calculate(i, constant(2.0f), constant(1.0f)).state_708, 2.0f);
}

S3_TEST(camera_relative_quadrants_and_output_raw_vector) {
    auto i = input();
    i.processed_stick_2692 = 0.6f;
    i.processed_stick_2688 = 0.8f;
    const auto right = calculate(i, constant(1.0f), constant(2.0f));
    near(right.state_708, 1.0f);
    near(right.state_712, 2.0f);
    CHECK_EQ(right.state_656, (std::array<float, 4>{0.6f, 0.0f, 0.8f, 0.0f}));
    i.processed_stick_2692 = -0.6f;
    near(calculate(i, constant(1.0f), constant(2.0f)).state_712, -2.0f);
}

S3_TEST(slope_is_flattened_before_input_transform) {
    const auto base = calculate(input(), angle_curve(), constant(1.0f));
    auto i = input();
    i.frame_forward_112 = {0.0f, 0.6f, 0.8f};
    const auto slope = calculate(i, angle_curve(), constant(1.0f));
    near(slope.state_708, base.state_708);
    near(slope.state_712, base.state_712);
}

S3_TEST(rotated_frame_changes_angle_but_not_stored_input) {
    auto i = input();
    i.frame_forward_112 = {1.0f, 0.0f, 0.0f};
    const auto result = calculate(i, angle_curve(), constant(1.0f));
    near(result.state_708, 0.5f);
    near(result.state_712, -1.0f);
    CHECK_EQ(result.state_656, (std::array<float, 4>{0.0f, 0.0f, 1.0f, 0.0f}));
}

S3_TEST(threshold_equality_uses_identity_and_below_uses_frame) {
    auto i = input();
    i.frame_forward_112 = {0.1f, s3::rs::from_bits(0x3f7fbe77u), 0.0f};
    near(calculate(i, angle_curve(), constant(1.0f)).state_708, 1.0f);
    i.frame_forward_112[1] = s3::rs::from_bits(0x3f7fbe76u);
    near(calculate(i, angle_curve(), constant(1.0f)).state_708, 0.5f);
}

S3_TEST(positive_vertical_identity_but_negative_vertical_has_no_invented_guard) {
    auto i = input();
    i.frame_forward_112 = {0.0f, 1.0f, 0.0f};
    near(calculate(i, constant(1.0f), constant(1.0f)).state_708, 1.0f);
    i.frame_forward_112[1] = -1.0f;
    CHECK(std::isnan(calculate(i, constant(1.0f), constant(1.0f)).state_708));
}

S3_TEST(zero_stick_is_zero_despite_intermediate_reciprocal_nan) {
    auto i = input();
    i.processed_stick_2688 = 0.0f;
    const auto result = calculate(i, constant(3.0f), constant(4.0f));
    CHECK_EQ(result.state_708, 0.0f);
    CHECK_EQ(result.state_712, 0.0f);
}

S3_TEST(magnitude_curve_and_processed_scales_all_contribute) {
    auto i = input();
    i.processed_stick_2692 = -3.0f;
    i.processed_stick_2688 = 4.0f;
    i.processed_scale_2912 = 2.0f;
    i.processed_scale_2908 = 3.0f;
    const auto result = calculate(i, constant(0.5f), constant(2.0f));
    near(result.state_708, 5.0f);
    near(result.state_712, -30.0f);
}

S3_TEST(shared_curve_exact_endpoints_knots_and_unordered_input) {
    PointGraph<8> curve;
    curve.x = {0.0f, 0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 1.0f};
    curve.y = {2.0f, 4.0f, 6.0f, 8.0f, 10.0f, 12.0f, 14.0f, 18.0f};
    for (std::size_t index = 0; index < 8; ++index) CHECK_EQ(curve.evaluate(curve.x[index]), curve.y[index]);
    CHECK_EQ(curve.evaluate(-1.0f), 2.0f);
    CHECK_EQ(curve.evaluate(2.0f), 18.0f);
    CHECK_EQ(curve.evaluate(NAN_), 18.0f);
    CHECK_EQ(curve.evaluate(0.0625f), 3.0f);
}

S3_TEST(original_angle_tracks_finite_atan_across_reduction_boundaries) {
    for (float boundary : {s3::rs::from_bits(0x39800000u), s3::rs::from_bits(0x3e8930a3u), 1.0f}) {
        const std::uint32_t b = s3::rs::to_bits(boundary);
        for (std::uint32_t bits : {b - 1, b, b + 1}) {
            const float value = s3::rs::from_bits(bits);
            near(math::rational_atan(value), std::atan(value));
            near(math::rational_atan(-value), std::atan(-value));
        }
    }
    for (float value : {0.0f, 0.1f, 0.5f, 2.0f, 10.0f, 1.0e30f}) near(math::rational_atan(value), std::atan(value));
}

S3_TEST(axis_override_preserves_x_sign_even_when_both_inputs_are_zero) {
    CHECK_EQ(s3::rs::to_bits(math::signed_angle(0.0f, 0.0f)), 0x3fc90fdbu);
    CHECK_EQ(s3::rs::to_bits(math::signed_angle(-0.0f, -0.0f)), 0xbfc90fdbu);
    near(math::signed_angle(1.0f, -1.0f), 3.0f * s3::rs::f32::consts::FRAC_PI_4);
    near(math::signed_angle(-1.0f, -1.0f), -3.0f * s3::rs::f32::consts::FRAC_PI_4);
}
}  // namespace
