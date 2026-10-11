// Port of skate-core/src/player/offboard/ground_entry/tests.rs.
#include "harness.hpp"
#include "s3/core/player/offboard/ground_entry.hpp"

namespace {
using namespace s3::player::offboard::ground_entry;
using math::IDENTITY;

Input input() {
    Input i;
    i.animation_frame = IDENTITY;
    i.processed_velocity_608 = {1.0f, 2.0f, 3.0f, 0.0f};
    i.processed_flags_2484 = 0;
    i.processed_flags_2476 = 0;
    i.requested_angle_2936 = 0.0f;
    i.requested_duration_2896 = 1.0f;
    i.previous_state_2504 = 0;
    i.previous_frame_up_208 = {0.0f, 1.0f, 0.0f, 0.0f};
    i.body_position_15872 = {0.0f, 1.0f, 0.0f, 0.0f};
    return i;
}

S3_TEST(ordinary_entry_uses_actual_animation_frame_and_rejects_vertical_velocity) {
    State state;
    auto i = input();
    i.animation_frame[3] = {7.0f, 8.0f, 9.0f, 0.0f};
    const auto placement = state.enter(i);
    CHECK_EQ(placement.frame, i.animation_frame);
    CHECK_EQ(placement.planar_velocity, (Vector{1.0f, 0.0f, 3.0f, 0.0f}));
    CHECK_EQ(placement.body_position, i.body_position_15872);
    CHECK(state.flags_144_to_150[5]);
    CHECK(!state.flags_144_to_150[6]);
}

S3_TEST(entry_resets_previous_flags_and_records_only_matching_previous_state) {
    State state;
    auto i = input();
    i.previous_state_2504 = 502;
    state.enter(i);
    CHECK(state.flags_144_to_150[4]);
    i.previous_state_2504 = 501;
    i.previous_frame_up_208[1] = 0.7f;
    state.enter(i);
    CHECK(state.flags_144_to_150[1]);
    CHECK(!state.flags_144_to_150[4]);
    i.previous_frame_up_208[1] = s3::rs::from_bits(0x3f35c28fu);
    state.enter(i);
    CHECK(!state.flags_144_to_150[1]);
}

S3_TEST(directed_entry_aligns_frame_to_velocity_and_preserves_translation) {
    State state;
    auto i = input();
    i.processed_flags_2484 = 0x4000;
    i.processed_velocity_608 = {3.0f, 2.0f, 0.0f, 0.0f};
    i.animation_frame[3] = {7.0f, 8.0f, 9.0f, 0.0f};
    const auto p = state.enter(i);
    CHECK(state.flags_144_to_150[6]);
    CHECK(s3::rs::abs(p.frame[2][0] - 1.0f) < 1e-6f);
    CHECK_EQ(p.frame[3], i.animation_frame[3]);
}

S3_TEST(low_speed_uses_animation_forward_and_mirroring_reverses_requested_turn) {
    State state;
    auto i = input();
    i.processed_flags_2484 = 0x4000;
    i.processed_velocity_608 = {};
    i.requested_angle_2936 = 0.5f;
    state.enter(i);
    const float normal = state.angular_velocity_176;
    i.processed_flags_2476 = 4;
    state.enter(i);
    CHECK(s3::rs::abs(state.angular_velocity_176 + normal) < 1e-5f);
    CHECK(normal > 0.0f);
}
}  // namespace
