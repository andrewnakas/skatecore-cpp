// Port of skate-core/src/animation/intent_filter/tests.rs.
#include "harness.hpp"
#include "s3/core/animation/intent_filter.hpp"

using namespace s3::animation::intent_filter;

namespace {
Settings settings() {
    Settings s;
    s.starting_value = 0.0f;
    s.default_value = 0.0f;
    s.scale = 1.0f;
    s.filters = {0, 0, 0, 0};
    s.ramp_time = std::nullopt;
    s.blend_rising = 0.5f;
    s.blend_falling = 0.25f;
    s.blend_out = 0.125f;
    s.clamp_velocity = std::nullopt;
    s.clamp_acceleration = std::nullopt;
    return s;
}
}  // namespace

S3_TEST(starting_value_bypasses_scale_and_filters_and_reentry_resets_history) {
    State s{4.0f, 9.0f, 7.0f};
    auto p = settings();
    p.starting_value = 2.0f;
    p.scale = 3.0f;
    p.filters = {1, 1, 1, 1};
    CHECK_EQ(s.begin(p), 2.0f);
    CHECK_EQ(s.elapsed, 0.0f);
    CHECK_EQ(s.previous_delta, 0.0f);
}

S3_TEST(missing_input_uses_blend_out_without_ramp_and_zero_is_present) {
    auto p = settings();
    p.ramp_time = 8.0f;
    State s{};
    s.value = 1.0f;
    CHECK_EQ(s.update(p, std::nullopt, 1.0f, {false, false}), 0.875f);
    s = State{};
    s.value = 1.0f;
    CHECK_EQ(s.update(p, 0.0f, 1.0f, {false, false}), 0.96875f);
}

S3_TEST(acceleration_precedes_velocity_and_limits_are_not_scaled_by_dt) {
    auto p = settings();
    p.blend_rising = 1.0f;
    p.clamp_acceleration = 0.25f;
    p.clamp_velocity = 0.125f;
    State s{};
    CHECK_EQ(s.update(p, 4.0f, 0.0f, {false, false}), 0.125f);
    CHECK_EQ(s.update(p, 4.0f, 3.0f, {false, false}), 0.25f);
    CHECK_EQ(s.update(p, -4.0f, 0.0f, {false, false}), 0.125f);
}

S3_TEST(scale_and_ordered_filters_use_live_stance) {
    auto p = settings();
    p.blend_rising = 1.0f;
    p.blend_falling = 1.0f;
    p.scale = 2.0f;
    p.filters = {1, 2, 3, 1};
    State s{};
    CHECK_EQ(s.update(p, 0.125f, 0.0f, {false, false}), 0.25f);
    CHECK_EQ(s.update(p, 0.125f, 0.0f, {true, true}), -0.75f);
}

S3_TEST(ramp_has_native_endpoint_and_signed_bit_ordering) {
    CHECK_EQ(ramp(0.0f, 2.0f), 0.0f);
    CHECK_EQ(ramp(0.5f, 2.0f), 0.25f);
    CHECK_EQ(ramp(2.0f, 2.0f), 1.0f);
    CHECK_EQ(ramp(3.0f, 2.0f), 1.0f);
    CHECK_EQ(ramp(-1.0f, 2.0f), 0.0f);
    CHECK_EQ(ramp(0.0f, 0.0f), 0.0f);
    CHECK_EQ(ramp(1.0f, 0.0f), 1.0f);
}
