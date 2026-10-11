// Port of the inline tests in skate-core/src/riding/grounded/state/output.rs.
#include "harness.hpp"
#include "s3/core/riding/grounded/state/output.hpp"

namespace {
using namespace s3::riding::grounded::state::output;

PhysicsGroundState state() {
    PhysicsGroundState s{};
    s.word_2560 = 0x11223344u;
    s.word_2564 = 0x55667788u;
    s.anti_flip_torque_2624 = {1.0f, 2.0f, 3.0f, 4.0f};
    s.steering_push_scalar_2640 = 1.0f;
    s.scalar_2664 = 6.0f;
    s.scalar_2668 = 7.0f;
    s.straighten_scale_2672 = 1.0f;
    s.flag_2720 = true;
    s.flag_2721 = true;
    s.anti_flip_nudge_applied_2723 = true;
    s.human_player_2724 = true;
    s.captured_position_valid_2726 = true;
    s.pinning_2727 = true;
    s.push_suppressed_2730 = true;
    s.flag_2731 = true;
    s.manual_correction_2732 = true;
    s.manual_opposition_2733 = true;
    return s;
}

GroundOutputFrame frame() {
    GroundOutputFrame f;
    f.axis_464 = {1.0f, 0.0f, 0.0f, 0.0f};
    f.velocity_608 = {5.0f, 2.0f, 3.0f, 4.0f};
    f.absolute_body_speed_2616 = 4.0f;
    f.deck_speed_2652 = 2.0f;
    f.state_timer_2664 = 0.0f;
    f.scalar_2720 = 8.0f;
    f.flags_2476 = 0x00400000u;
    f.flags_2484 = 0x00002000u;
    f.selected_mode_flag_109 = true;
    return f;
}

GroundOutputSettings settings() { return GroundOutputSettings{{2.0f, 3.0f}, 3.0f}; }

S3_TEST(fill_maps_every_unconditional_field_and_projection_branch) {
    const auto output = fill_physics_output(state(), frame(), settings());

    CHECK_EQ(output.skateboard_motion_4, (SkateboardMotionOutput{true, true}));
    CHECK(output.velocity_projection_36 ==
          std::optional<GroundVelocityProjectionOutput>(
              GroundVelocityProjectionOutput{{0.0f, 2.0f, 3.0f, 4.0f}, true}));
    CHECK(output.ground_32.wall_ride_exit);
    CHECK(output.ground_32.anti_flip_nudge_present);
    CHECK(output.ground_32.is_pinning);
    CHECK_EQ(output.ground_32.anti_flip_torque, (Vector4{1.0f, 2.0f, 3.0f, 4.0f}));
    CHECK_EQ(output.ground_32.time_to_skitch, 6.0f);
    CHECK_EQ(output.ground_32.skitch_spline_height, 7.0f);
    CHECK_EQ(output.ground_32.processed_scalar_2720, 8.0f);
    CHECK(output.ground_32.processed_flag_2484_bit_13);
    CHECK_EQ(output.state_28.grab_spline_type, 0x11223344u);
    CHECK_EQ(output.state_28.grab_spline_object_id, 0x55667788u);
    CHECK(output.state_28.flag_84);
    CHECK(output.state_28.has_world_grab_intent_without_object);
    CHECK(output.state_28.manual_correction_write_78 == std::optional<bool>(true));
    CHECK(output.intents_52.has_world_grab_intent);
    CHECK(output.intents_52.selected_mode_below_speed_threshold_58);
    CHECK(!output.is_grabbing_object_72_304);
    CHECK(output.manual_opposition_56_168);
    CHECK(output.push_suppressed_20_596);
}

S3_TEST(fill_preserves_native_conditional_writes_and_unordered_comparisons) {
    auto s = state();
    s.flag_2729 = true;
    s.manual_correction_2732 = false;
    auto f = frame();
    f.state_timer_2664 = s3::rs::f32::NAN_;
    f.absolute_body_speed_2616 = s3::rs::f32::NAN_;
    f.deck_speed_2652 = s3::rs::f32::NAN_;

    const auto output = fill_physics_output(s, f, settings());

    CHECK(!output.velocity_projection_36.has_value());
    CHECK(!output.skateboard_motion_4.is_at_pushable_speed);
    CHECK(!output.intents_52.selected_mode_below_speed_threshold_58);
    CHECK(!output.state_28.has_world_grab_intent_without_object);
    CHECK(!output.state_28.manual_correction_write_78.has_value());
}

S3_TEST(fill_extracts_only_the_two_recovered_processed_bits) {
    auto f = frame();
    f.flags_2476 = 0x00200000u;
    f.flags_2484 = 0x00001000u;
    f.state_timer_2664 = -0.0f;

    const auto output = fill_physics_output(state(), f, settings());

    CHECK(!output.intents_52.has_world_grab_intent);
    CHECK(!output.ground_32.processed_flag_2484_bit_13);
    CHECK(output.velocity_projection_36.has_value());
}

}  // namespace
