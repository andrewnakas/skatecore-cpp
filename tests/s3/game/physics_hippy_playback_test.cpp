// Port of skate-game/src/tests/hippy_playback.rs (mounted in physics.rs as hippy_tests).
//! Real raw input, stock graphs, shared physical solve and camera; no state injection.
// `#[ignore = "requires private stock graph/animation/physics assets"]`: runs on the cleanroom pack,
// skips without it.
#include <cmath>
#include <cstdio>
#include <limits>
#include <optional>

#include "physics_offboard_test_helpers.hpp"

using namespace s3test::game::offboard_playback;

namespace {

void run_jump(std::size_t hold, bool pushing, std::size_t repetitions) {
    auto loaded = Playback::load("normal", true);
    if (!loaded) return;
    auto& r = *loaded;
    auto& physics = r.physics;
    auto& skater = r.skater;
    auto& camera = r.camera;
    bool entered = false;
    bool descended = false;
    bool landed = false;
    float peak = -std::numeric_limits<float>::infinity();
    float launch_height = 0.0f;
    std::size_t landing_ticks = 0;
    bool previous_landing = false;
    std::size_t landing_count = 0;
    std::optional<s3::math::Vector3> board_launch;
    float rolling_distance = 0.0f;
    for (std::size_t tick = 0; tick < 180 * repetitions + 120; ++tick) {
        const bool charge = tick >= 45 && tick < 45 + 180 * repetitions && (tick - 45) % 180 < hold;
        std::uint16_t buttons = 0;
        if (charge) {
            buttons = 0x5000;
        } else if (pushing && tick >= 12 && tick < 32) {
            buttons = 0x1000;
        }
        auto actions = r.sample_update(xbox(buttons, {0, 0}, {0, 0}, {0, 0}), true);
        const auto advanced = r.advance(actions);
        CHECK_MSG(advanced.is_ok(), "Hippy tick=" << tick << ": " << advanced.unwrap_err());
        const auto state = skater.player_state.current();
        const float height = skater.skeleton.record.centre_of_mass[1];
        CHECK_MSG(std::isfinite(height), "nonfinite physical COM at " << tick);
        if (state == PhysicalStateId::LandingOnDeck) {
            if (!previous_landing) board_launch = physics.board.bodies()[6].rates.position;
            if (board_launch) {
                const auto start = *board_launch;
                const auto now = physics.board.bodies()[6].rates.position;
                const float dx = now.x - start.x;
                const float dz = now.z - start.z;
                rolling_distance = s3::rs::max(rolling_distance, std::sqrt(dx * dx + dz * dz));
            }
            if (!entered) launch_height = height;
            entered = true;
            landing_ticks += 1;
            peak = s3::rs::max(peak, height);
            descended = descended || height < peak - 0.1f;
            CHECK(skater.landing_deck.manager.trajectory_valid_164);
            CHECK_EQ(skater.ground_lifecycle.board_animated_290, 0);
        } else if (entered && state == PhysicalStateId::PhysicsGround) {
            landed = true;
            if (previous_landing) landing_count += 1;
            CHECK(!skater.landing_deck.manager.trajectory_valid_164);
        }
        if (tick % 40 == 0 || previous_landing != (state == PhysicalStateId::LandingOnDeck)) {
            std::fprintf(stderr, "hippy tick=%zu state=%u com_y=%g time=%g can_land=%d near=%d\n", tick,
                         static_cast<unsigned>(state), static_cast<double>(height),
                         static_cast<double>(skater.landing_on_deck.state.time_to_land),
                         static_cast<int>(skater.landing_deck.fill().can_land_316),
                         static_cast<int>(skater.landing_on_deck.state.near_deck));
        }
        previous_landing = state == PhysicalStateId::LandingOnDeck;
        CHECK_EQ(skater.pose_generation, physics.ticks);
        CHECK(camera.frame.has_value());
    }
    CHECK_MSG(entered, "Raw A+X never entered stock hippy graph/state503");
    CHECK_MSG(peak > launch_height + 0.15f, "physical rider never rose: " << launch_height << " -> " << peak);
    CHECK_MSG(descended, "physical rider did not descend from apex");
    CHECK_MSG(landed, "physical rider never returned to riding; final="
                          << static_cast<unsigned>(skater.player_state.current()));
    CHECK_MSG(landing_count == repetitions,
              "hold=" << hold << " pushing=" << pushing << ": missing re-entry/landing (" << landing_count << ")");
    CHECK_MSG(landing_ticks > 5 && landing_ticks < 100 * repetitions, "landing state stalled: " << landing_ticks);
    if (pushing) CHECK_MSG(rolling_distance > 0.1f, "board stopped during rolling hippy jump");
}

}  // namespace

// #[ignore = "requires private stock graph/animation/physics assets"]
S3_TEST(raw_controller_hippy_jump_returns_to_riding) { run_jump(25, false, 1); }

// #[ignore = "requires private stock graph/animation/physics assets"]
S3_TEST(hippy_charge_repeat_and_rolling_gameplay) {
    for (const std::size_t hold : {std::size_t(12), std::size_t(60)}) run_jump(hold, false, 1);
    run_jump(25, false, 2);
    run_jump(25, true, 1);
}
