// Port of skate-core/src/player/offboard/twist_lean/tests.rs.
#include "harness.hpp"
#include "s3/core/player/offboard/twist_lean.hpp"

namespace {
using namespace s3::player::offboard::twist_lean;
std::optional<Observation> observation(float right, float up, bool mirrored) { return Observation{right, up, mirrored}; }

S3_TEST(capture_preserves_twist_and_lean_after_physics_and_stance_change) {
    State state{};
    state.begin(observation(0.25f, 0.75f, true));
    CHECK(state.update(false, observation(1.5f, 2.0f, false)) == (State{-0.75f, 0.25f}));
    state.begin(observation(1.5f, 2.0f, false));
    CHECK(state.update(false, std::nullopt) == (State{2.0f, 1.5f}));
}

S3_TEST(always_reads_live_angles_without_changing_capture) {
    State state{};
    state.begin(observation(0.25f, 0.75f, false));
    CHECK(state.update(true, observation(1.5f, 2.0f, true)) == (State{-2.0f, 1.5f}));
    CHECK(state.update(false, std::nullopt) == (State{0.75f, 0.25f}));
}

S3_TEST(missing_original_components_preserve_capture_but_live_locals_start_zero) {
    State state{};
    state.begin(observation(0.25f, 0.75f, false));
    state.begin(std::nullopt);
    CHECK(state.update(false, std::nullopt) == (State{0.75f, 0.25f}));
    CHECK(state.update(true, std::nullopt) == State{});
}
}  // namespace
