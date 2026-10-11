// Port of skate-game/src/graph_host/motion_offboard/cadence/tests.rs.
#include "harness.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/game/graph_host/motion_offboard_cadence.hpp"

using namespace s3::game::graph_host::motion_offboard_cadence;
using s3::animation::playback_parameters::SettableAttributes;
using s3::animation::skeleton_input::name::encode;

S3_TEST(biped_updates_the_existing_phase_only_during_update) {
    float phase = 0.25f;
    biped_cadence(0.8f, &phase, 0);
    CHECK_EQ(phase, 0.25f);
    biped_cadence(0.8f, &phase, 1);
    CHECK_EQ(phase, 0.8f);
    biped_cadence(std::nullopt, &phase, 1);
    biped_cadence(0.1f, &phase, 2);
    CHECK_EQ(phase, 0.8f);
}

S3_TEST(match_captures_begin_and_republishes_without_consuming_it) {
    MatchCadence state;
    SettableAttributes sink;
    state.begin(0.73f, true);
    CHECK(state.pending);
    state.update(&sink);
    CHECK(!state.pending);
    const auto value = sink.entries()[0];
    CHECK(value.name == encode("CadenceStartPercent"));
    CHECK_EQ(value.value, 0.73f);
    CHECK(!value.normalized);
    CHECK_EQ(value.sequence_id, -1);
    sink.clear();
    state.end();
    state.update(&sink);
    CHECK_EQ(sink.entries()[0].value, 0.73f);
}

S3_TEST(absent_components_preserve_retained_capture_and_pending_flag) {
    MatchCadence state;
    state.begin(0.4f, true);
    state.begin(0.9f, false);
    state.begin(std::nullopt, true);
    state.update(nullptr);
    CHECK_EQ(state.captured_phase, 0.4f);
    CHECK(state.pending);
    state.end();
    CHECK(state.pending);
}

S3_TEST(native_initial_capture_is_zero_before_any_begin) {
    MatchCadence state;
    SettableAttributes sink;
    state.update(&sink);
    CHECK_EQ(sink.entries()[0].value, 0.0f);
}

S3_TEST(locomotion_comparison_does_not_clamp_unknown_physical_values) {
    for (std::uint32_t expected = 0; expected < 4; ++expected) {
        const LocoState condition{expected};
        for (std::uint32_t actual = 0; actual < 5; ++actual) {
            CHECK_EQ(condition.evaluate(actual), expected == actual);
        }
    }
}

S3_TEST(missing_locomotion_config_is_not_silently_stand) {
    const std::vector<s3::data::GraphAttribute> none;
    CHECK(LocoState::parse(Attributes(none)).is_err());
}
