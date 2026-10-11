// Port of the #[cfg(test)] module in skate-game/src/graph_host/outputs.rs.
#include "harness.hpp"
#include "s3/game/graph_host/outputs.hpp"

using namespace s3::game::graph_host::outputs;

S3_TEST(output_captures_named_turning_without_mutable_map_handoff) {
    IntentMap intents;
    intents.insert("Turn", 0.25f);
    intents.insert("RawTurn", -0.5f);
    intents.insert("HardTurn", 1.0f);
    const ActionGraphOutput output = ActionGraphOutput::from_host(7, IntentMap{}, intents, {});
    CHECK_EQ(output.tick, std::uint64_t(7));
    CHECK_EQ(output.turning.turn, 0.25f);
    CHECK_EQ(output.turning.raw_turn, -0.5f);
    CHECK_EQ(output.turning.hard_turn, 1.0f);
    CHECK(output.controls.authored_values().is_empty());
}

S3_TEST(output_projects_action_family_controls) {
    IntentMap action;
    action.insert("WipeOutRequest", 1.0f);
    action.insert("OB_AirBodyTweakX", 0.25f);
    action.insert("OB_AirBodyTweakY", -0.5f);
    action.insert("WipeoutGestureX", 0.75f);
    action.insert("WipeoutGestureY", -0.25f);
    const ActionGraphOutput output = ActionGraphOutput::from_host(3, action, IntentMap{}, {});
    CHECK(output.controls.wipeout.request);
    CHECK(output.controls.wipeout.air_body_tweak == (std::array<float, 2>{0.25f, -0.5f}));
    CHECK(output.controls.wipeout.gesture == (std::optional<std::array<float, 2>>(std::array<float, 2>{0.75f, -0.25f})));
}

S3_TEST(graph_diagnostics_are_bounded_and_tick_resettable) {
    GraphDiagnostics diagnostics;
    for (int index = 0; index < 65; ++index) diagnostics.push("diagnostic " + std::to_string(index));
    CHECK(!diagnostics.is_empty());
    CHECK(diagnostics.join("\n").find("truncated") != std::string::npos);
    diagnostics.clear();
    CHECK(diagnostics.is_empty());
}
