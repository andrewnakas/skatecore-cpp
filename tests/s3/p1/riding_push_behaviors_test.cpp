// Port of skate-core/src/riding/tests/push_behaviors.rs.
#include "harness.hpp"
#include "s3/core/riding/push_behaviors.hpp"

namespace {
using namespace s3::riding::push_behaviors;
using s3::point_graph::PointGraph;

PushAnimationCurves curves() {
    PointGraph<8> linear;
    for (std::size_t i = 0; i < 8; ++i) {
        linear.x[i] = float(i) / 7.0f;
        linear.y[i] = float(i);
    }
    PointGraph<8> constant;
    constant.x = linear.x;
    constant.y.fill(1.0f);
    return PushAnimationCurves{constant, linear, constant, constant};
}
PushState state() {
    const PushBlendParameters zero{0.0f, 0.0f, 0.0f};
    return PushState{0.375f, 0.0f, zero, zero, false};
}
PushAttributes clips() {
    return PushAttributes::from_clips({
        PushClipMetrics{1.0f, 0.0f, 4.0f},
        PushClipMetrics{2.0f, 4.0f, 8.0f},
        PushClipMetrics{1.0f, 0.0f, 1.0f},
        PushClipMetrics{2.0f, 4.0f, 5.0f},
    });
}
#define NEAR(a, b) CHECK_MSG(s3::rs::abs((a) - (b)) < 0.000002f, (a) << " != " << (b))

S3_TEST(unequal_clip_lengths_require_distance_weighted_velocity_blend) {
    const auto target = clips().target(2.0f, 2.5f);
    // At coefficient1/3, equal weighted durations make the begin velocity2.
    // The strong/gentle end velocities are6 and3, so their midpoint is4.5.
    NEAR(target.hstr_vel_b, 1.0f / 3.0f);
    NEAR(target.lstr_vel_b, 1.0f / 3.0f);
    NEAR(target.vel_e, 0.5f);
}
S3_TEST(authored_bounds_cap_begin_speed_and_strength_before_blending) {
    const auto attributes = clips();
    CHECK_EQ(attributes.minimum_begin_velocity, 0.0f);
    CHECK_EQ(attributes.maximum_begin_velocity, 4.0f);
    CHECK_EQ(attributes.minimum_delta_velocity, 1.0f);
    CHECK_EQ(attributes.maximum_delta_velocity, 4.0f);
    CHECK_EQ(attributes.target(-5.0f, -3.0f), (PushBlendParameters{0.0f, 0.0f, 0.0f}));
    CHECK_EQ(attributes.target(10.0f, 20.0f), (PushBlendParameters{1.0f, 1.0f, 1.0f}));
}
S3_TEST(out_factor_combines_normalized_speed_strength_then_caps_at_stock_limit) {
    NEAR(clips().out_factor(2.5f, 2.5f, 0.4f, 0.659f), 0.5f);
    CHECK_EQ(clips().out_factor(50.0f, 50.0f, 0.4f, 0.659f), 0.659f);
    CHECK_EQ(clips().out_factor(-20.0f, -20.0f, 0.4f, 0.659f), 0.0f);
}
S3_TEST(init_push_preserves_out_factor_and_marks_all_blends_uninitialized) {
    auto s = state();
    s.current_push_dv = 4.0f;
    s.continue_push = true;
    s.initialize_push();
    CHECK_EQ(s.out_factor, 0.375f);
    CHECK_EQ(s.current_push_dv, 0.0f);
    CHECK(!s.continue_push);
    CHECK(s.current == s.target);
    CHECK_EQ(s.current.vel_e, -1.0f);
}
S3_TEST(teleport_push_evaluates_previous_held_time_before_advancing) {
    auto first = FirstPushStrength::begin(0.1f, 0.5f);
    auto s = state();
    first.update(s, curves(), std::nullopt, 0.0f, 0.25f);
    CHECK_EQ(s.current_push_dv, 0.0f);
    CHECK_EQ(first.simulated_held_seconds, 0.25f);
    CHECK(!first.first_push && !first.first_update);
    first.update(s, curves(), std::nullopt, 0.0f, 0.25f);
    NEAR(s.current_push_dv, 1.75f);
}
S3_TEST(first_push_release_latches_until_fresh_activation) {
    auto first = FirstPushStrength::begin(0.5f, 0.5f);
    CHECK(!first.push_from_teleport);
    auto s = state();
    first.update(s, curves(), 0.25f, 0.0f, 0.1f);
    const float strength = s.current_push_dv;
    first.update(s, curves(), std::nullopt, 0.0f, 0.1f);
    first.update(s, curves(), 1.0f, 0.0f, 0.1f);
    CHECK_EQ(s.current_push_dv, strength);
}
S3_TEST(holding_first_cap_can_reduce_strength_but_next_hold_is_uncapped) {
    auto s = state();
    s.current_push_dv = 6.0f;
    auto cycle = PushCycle::begin(s, curves(), PushIntents{1.0f, false, true});
    cycle.update(s, curves(), PushIntents{1.0f, false, true}, 0.0f, 4.0f);
    CHECK_EQ(s.current_push_dv, 4.0f);
    cycle.update(s, curves(), PushIntents{}, 0.0f, 4.0f);
    CHECK(!s.continue_push);
    cycle.update(s, curves(), PushIntents{1.0f, true, true}, 0.0f, 4.0f);
    CHECK_EQ(s.current_push_dv, 7.0f);
    CHECK(cycle.phase == PushCyclePhase::HoldingNext);
    s.current_push_dv = 9.0f;
    cycle.update(s, curves(), PushIntents{}, 0.0f, 4.0f);
    CHECK_EQ(s.current_push_dv, 7.0f);
    CHECK(cycle.phase == PushCyclePhase::NextReleased);
    // Release does not clear the continuation set in the previous next-hold.
    CHECK(s.continue_push);
}
S3_TEST(released_first_and_new_push_checks_can_cascade_in_one_update) {
    auto s = state();
    auto cycle = PushCycle::begin(s, curves(), PushIntents{0.25f, false, true});
    cycle.update(s, curves(), PushIntents{std::nullopt, true, true}, 0.0f, 4.0f);
    CHECK(cycle.phase == PushCyclePhase::HoldingNext);
    CHECK(s.continue_push);
    CHECK_EQ(cycle.next_push_dv, 0.0f);
}
S3_TEST(out_distance_uses_live_foot_and_deck_axes_with_only_z_flipped) {
    PushFootFrame frame{
        {11.0f, 22.0f, 33.0f, 0.0f}, {14.0f, 25.0f, 36.0f, 0.0f}, {10.0f, 20.0f, 30.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},    {1.0f, 0.0f, 0.0f, 0.0f},    false,
    };
    CHECK_EQ(frame.out_distance(false), (std::array<float, 2>{1.0f, 2.0f}));
    CHECK_EQ(frame.out_distance(true), (std::array<float, 2>{4.0f, 5.0f}));
    frame.skateboard_flipped = true;
    CHECK_EQ(frame.out_distance(true), (std::array<float, 2>{-4.0f, 5.0f}));
}

}  // namespace
