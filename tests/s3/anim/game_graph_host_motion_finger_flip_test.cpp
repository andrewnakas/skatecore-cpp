// Port of the #[cfg(test)] mod tests in skate-game/src/graph_host/motion_finger_flip.rs.
#include <cmath>

#include "harness.hpp"
#include "s3/game/graph_host/motion_finger_flip.hpp"

using namespace s3::game::graph_host::motion_finger_flip;

S3_TEST(finger_flip_release_regrab_bounds_and_reentry) {
    Settings settings;
    settings.minimum = 0.0f;
    settings.maximum = 0.5f;
    settings.curve.x = s3::rs::from_fn<8>([](std::size_t i) { return static_cast<float>(i) / 14.0f; });
    settings.curve.y = s3::rs::from_fn<8>([](std::size_t i) { return 1.0f - static_cast<float>(i) / 7.0f; });
    State state;
    state.begin();
    CHECK_EQ(state.update(true, 0.25f, settings), 1.0f);
    CHECK(std::fabs(state.update(false, 0.125f, settings) - 0.75f) < 1e-6f);
    CHECK(std::fabs(state.update(false, 0.125f, settings) - 0.5f) < 1e-6f);
    CHECK(std::fabs(state.update(true, 0.125f, settings) - 0.75f) < 1e-6f);
    CHECK_EQ(state.update(false, 1.0f, settings), 0.0f);
    CHECK_EQ(state.update(false, 1.0f, settings), 0.0f);
    CHECK_EQ(state.elapsed, 0.5f);
    state.begin();
    CHECK_EQ(state.elapsed, 0.0f);
    CHECK_EQ(state.update(true, 0.25f, settings), 1.0f);
}
