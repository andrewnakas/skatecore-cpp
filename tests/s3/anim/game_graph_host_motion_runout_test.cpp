// Port of skate-game/src/graph_host/motion_offboard/runout/tests.rs.
#include <cmath>

#include "harness.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/game/graph_host/motion_runout.hpp"

using namespace s3::game::graph_host::motion_runout;
using s3::animation::playback_parameters::SettableAttributes;
using s3::animation::skeleton_input::name::encode;

namespace {
Observation observation() {
    Observation o;
    o.offboard_flag_331 = true;
    o.offboard_velocity_128 = {3.0f, 0.0f, 4.0f, 91.0f};
    o.reckoning_velocity_16 = {0.0f, 0.0f, 2.0f, 0.0f};
    o.reckoning_up_96 = {0.0f, 1.0f, 0.0f, 0.0f};
    o.skeleton_vector_0 = {0.0f, 0.0f, 1.0f, 0.0f};
    o.animation_mirrored = false;
    return o;
}
}  // namespace

S3_TEST(offboard_selection_and_mirror_are_sampled_at_begin) {
    State state;
    state.begin(observation());
    CHECK(std::fabs(s3::rs::unwrap(state.speed_12) - 5.0f) < 0.0001f);
    const float angle = state.angle_degrees_8;
    CHECK(angle > 36.0f && angle < 38.0f);
    Observation mirrored = observation();
    mirrored.animation_mirrored = true;
    state.begin(mirrored);
    CHECK(std::fabs(state.angle_degrees_8 + angle) < 0.0001f);
    mirrored.offboard_flag_331 = false;
    state.begin(mirrored);
    CHECK(std::fabs(s3::rs::unwrap(state.speed_12) - 2.0f) < 0.0001f);
    CHECK(std::fabs(state.angle_degrees_8) < 0.01f);
}

S3_TEST(update_reuses_capture_and_preserves_queue_order_and_flags) {
    State state;
    state.begin(observation());
    const State captured = state;
    SettableAttributes sink;
    state.update(&sink).unwrap();
    state.begin(std::nullopt);
    state.update(&sink).unwrap();
    CHECK(state == captured);
    const auto entries = sink.entries();
    CHECK_EQ(entries.len(), std::size_t(2));
    CHECK(entries[0].name == encode("BipedStartAngle"));
    CHECK(entries[1].name == encode("BipedSpeed"));
    CHECK_EQ(entries[0].value, captured.angle_degrees_8);
    CHECK_EQ(entries[1].value, s3::rs::unwrap(captured.speed_12));
    for (const auto& entry : entries) CHECK(!entry.normalized && entry.sequence_id == -1);
}

S3_TEST(absent_begin_does_not_fabricate_speed_and_stationary_begin_is_valid) {
    State state;
    SettableAttributes sink;
    CHECK(state.update(&sink).is_err());
    CHECK(sink.entries().is_empty());
    state.update<SettableAttributes>(nullptr).unwrap();
    Observation input = observation();
    input.offboard_velocity_128 = {0.0f, 0.0f, 0.0f, 0.0f};
    state.begin(input);
    CHECK_EQ(state.angle_degrees_8, 0.0f);
    CHECK(state.speed_12 == std::optional<float>(0.0f));
    state.update(&sink).unwrap();
}
