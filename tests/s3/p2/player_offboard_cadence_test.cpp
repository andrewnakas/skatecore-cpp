// Port of skate-core/src/player/offboard/cadence/tests.rs.
#include "harness.hpp"
#include "s3/core/player/offboard/cadence.hpp"

namespace {
using namespace s3::player::offboard::cadence;

void close(float actual, float expected) { CHECK_MSG(std::fabs(actual - expected) < 2.0e-6f, actual << " != " << expected); }
CadenceInput input() {
    CadenceInput i;
    i.motion_512 = {0.0f, 0.0f, 1.0f};
    i.motion_reference_272 = {};
    i.reject_axis_400 = {0.0f, 0.0f, 1.0f};
    i.reject_enabled_708 = false;
    i.up_144 = {0.0f, 1.0f, 0.0f};
    i.frame_rows_0_16_32 = {{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
    i.frame_position_48 = {};
    i.animation_motion_224 = {0.0f, 0.0f, 1.0f};
    i.requested_duration_288 = 0.0f;
    i.requested_phase_296 = -1.0f;
    i.suppress_adjustment_353 = false;
    i.contact_flags_176 = 0;
    i.contact_point_96 = {0.0f, 0.0f, 1.2f};
    return i;
}
CadenceThresholds thresholds() { return CadenceThresholds::from_clip_speeds(2.0f, 4.0f, 6.0f); }

S3_TEST(stock_initialization_and_clip_thresholds) {
    BipedCadence c{};
    CHECK(!c.phase.duration.has_value());
    CHECK_EQ(c.phase.target, -1.0f);
    CHECK_EQ(thresholds()._0, (std::array<float, 4>{0.01f, 3.0f, 5.0f, 1.0e10f}));
}

S3_TEST(moving_rate_uses_physical_motion_relative_to_animation) {
    BipedCadence c{};
    auto i = input();
    i.motion_512[2] = 4.0f;
    i.motion_reference_272[2] = 1.0f;
    i.animation_motion_224[2] = 2.0f;
    c.update(i, thresholds());
    CHECK_EQ(c.locomotion_index, 1u);  // Exact threshold chooses lower bucket.
    close(c.phase.rate, 1.5f);
    close(c.phase.phase, 0.025f);
}

S3_TEST(movement_beyond_every_threshold_retains_previous_locomotion) {
    BipedCadence c{};
    c.locomotion_index = 3;
    c.update(input(), CadenceThresholds{{0.0f, 0.0f, 0.0f, 0.0f}});
    CHECK_EQ(c.locomotion_index, 3u);
}

S3_TEST(local_motion_keeps_vertical_component_for_cadence) {
    BipedCadence c{};
    auto i = input();
    i.motion_512 = {2.0f, 3.0f, 0.0f};
    i.frame_rows_0_16_32 = {{{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}}};
    c.update(i, thresholds());
    close(c.phase.rate, std::sqrt(13.0f));
    CHECK_EQ(c.locomotion_index, 1u);
}

S3_TEST(contact_rejection_can_stop_locomotion_without_overwriting_animation) {
    BipedCadence c{};
    c.phase.phase = 0.25f;
    auto i = input();
    i.reject_enabled_708 = true;
    c.update(i, thresholds());
    CHECK_EQ(c.locomotion_index, 0u);
    CHECK_EQ(c.phase.target, 0.0f);  // Equidistant stop targets prefer first.
    CHECK(c.phase.duration == std::optional<float>(0.2f));
    close(c.phase.rate, 1.25f);
    close(c.phase.phase, 0.25f - 1.25f / 60.0f);
}

S3_TEST(repeated_explicit_target_does_not_recompute_rate_each_tick) {
    BipedCadence c{};
    c.phase.phase = 0.9f;
    auto i = input();
    i.requested_phase_296 = 0.1f;
    i.requested_duration_288 = 0.3f;
    c.update(i, thresholds());
    const float rate = c.phase.rate;
    close(rate, 1.0f);
    c.update(i, thresholds());
    CHECK_EQ(c.phase.rate, rate);
    close(c.phase.phase, 0.9f + 2.0f / 60.0f);
}

S3_TEST(forward_snap_preserves_one_endpoint_and_free_motion_wraps) {
    BipedPhase phase{};
    phase.phase = 0.99f;
    phase.rate = 2.0f;
    phase.target = 1.0f;
    phase.forward_target = true;
    phase.advance();
    CHECK_EQ(phase.phase, 1.0f);
    phase.advance();
    CHECK_EQ(phase.phase, 1.0f);
    phase.target = -1.0f;
    phase.advance();
    close(phase.phase, 2.0f / 60.0f);
}

S3_TEST(closed_wrap_endpoints_are_not_euclidean_remainder) {
    CHECK_EQ(wrap(0.0f, 1.0f, 1.0f), 1.0f);
    CHECK_EQ(wrap(0.0f, -1.0f, 1.0f), 1.0f);
    CHECK_EQ(wrap(0.0f, 2.0f, 1.0f), 0.0f);
    CHECK_EQ(wrap(-0.5f, 0.5f, 0.5f), 0.5f);
    CHECK_EQ(wrap(-0.5f, -0.5f, 0.5f), -0.5f);
}

S3_TEST(nearest_target_can_advance_backwards_across_zero) {
    BipedPhase phase{};
    phase.phase = 0.01f;
    phase.rate = 0.3f;
    phase.target = 0.99f;
    phase.advance();
    close(phase.phase, 0.005f);
}

S3_TEST(obstacle_target_interval_preserves_authored_boundary_ties) {
    const std::pair<float, float> cases[] = {{0.37f, 0.03f}, {0.4f, 0.5f}, {0.87f, 0.03f}, {0.88f, 0.52f}};
    for (auto [phase, expected_rate] : cases) {
        BipedCadence c{};
        c.phase.phase = phase;
        auto i = input();
        i.contact_flags_176 = 0x100;
        c.update(i, thresholds());
        close(c.phase.rate, expected_rate);
        CHECK_EQ(c.phase.target, -1.0f);
    }
}

S3_TEST(obstacle_contact_chooses_rate_near_current_and_suppression_skips_it) {
    BipedCadence c{};
    auto i = input();
    i.contact_flags_176 = 0x80;
    i.contact_point_96[2] = 0.7f;  // Half second to contact after0.2 clearance.
    c.update(i, thresholds());
    close(c.phase.rate, 0.9f);
    i.suppress_adjustment_353 = true;
    c.update(i, thresholds());
    close(c.phase.rate, 1.0f);
}

S3_TEST(rate_selection_uses_inclusive_limits_and_clamps_nearest_candidate) {
    CHECK_EQ(select_rate(1.0f, 2.0f, 1.0f, 2.0f, 2.0f), 2.0f);
    CHECK_EQ(select_rate(0.0f, 2.0f, 1.0f, 2.0f, 0.0f), 2.0f);
    CHECK_EQ(select_rate(0.0f, 4.0f, 1.0f, 3.0f, 0.0f), 3.0f);  // Equal distances prefer B.
    CHECK_EQ(select_rate(0.5f, 5.0f, 1.0f, 3.0f, 0.0f), 1.0f);
}

S3_TEST(no_animation_motion_gives_zero_rate_without_fabricated_clip_speed) {
    BipedCadence c{};
    auto i = input();
    i.animation_motion_224 = {};
    c.update(i, thresholds());
    CHECK_EQ(c.phase.rate, 0.0f);
    CHECK_EQ(c.phase.phase, 0.0f);
    CHECK_EQ(c.locomotion_index, 1u);
}
}  // namespace
