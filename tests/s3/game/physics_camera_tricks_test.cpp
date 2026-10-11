// Port of skate-game/src/physics/camera_tricks.rs (`#[ignore = "requires private stock graphs and
// animation assets"]`). In the reference crate this file is not mounted by any `mod`, so the Rust
// test never compiles; here it reads the cleanroom pack (skips without it) and, like the Rust
// `#[ignore]`, only runs when S3_RUN_IGNORED is set.
#include <cstdlib>
#include <string>

#include "game_test_assets.hpp"
#include "harness.hpp"
#include "s3/data/manifest.hpp"
#include "s3/game/camera.hpp"
#include "s3/game/difficulty.hpp"
#include "s3/game/graph_runtime.hpp"
#include "s3/game/input/controllers.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/controls.hpp"
#include "s3/game/physics/frame.hpp"
#include "s3/game/physics/skater.hpp"

using namespace s3::game;
using namespace s3::game::physics;

static void replay(std::array<std::int16_t, 2> flick) {
    const auto* vfs = s3test::game::assets();
    if (vfs == nullptr) return;
    const std::string& root = s3test::game::ASSET_ROOT;
    auto assets = s3::data::GameAssets::load(*vfs, root);
    CHECK(assets.ok);
    if (!assets.ok) return;
    auto graphs = graph_runtime::StockGraphs::load(*vfs, root, assets.value).unwrap();
    auto physics = GamePhysics::load_with_difficulty(*vfs, root, nullptr, difficulty::Difficulty::Normal).unwrap();
    auto skater = SkaterRuntime::load(*vfs, root, graphs, physics, "normal").unwrap();
    auto camera = camera::CameraRuntime::load(*vfs, root).unwrap();
    input::controllers::ControllerInput input{};
    auto controls = PlayerControls::load(*vfs, root).unwrap();
    bool saw_air = false;
    bool saw_air_shot = false;
    float max_air_height = 0.0f;
    for (int tick = 0; tick < 300; ++tick) {
        s3::input::xbox::XboxState state{};
        state.buttons = 0;
        if (tick >= 120 && tick < 150) {
            state.right = {0, -32767};
        } else if (tick >= 150 && tick < 153) {
            state.right = flick;
        }
        input.sample_raw_for_test(state);
        auto actions = input.player_actions();
        controls.update(actions, physics.settings.step.simulation.time_step, physics.settings.input_magnitude_threshold,
                        skater.player_input.physical.scoring.capabilities_204);
        controls.publish_gestures(physics.animation_profile.physics_mode,
                                  skater.player_input.physical.state.state_16);
        const auto advanced = frame::advance(physics, skater, controls, graphs, actions, true, camera);
        CHECK_MSG(advanced.is_ok(), (advanced.is_ok() ? std::string() : advanced.unwrap_err()));
        if (!advanced.is_ok()) return;
        const bool air = skater.player_input.physical.air.known_air_valid_437 != 0;
        if (air) {
            saw_air = true;
            saw_air_shot = saw_air_shot || camera.selected_shot() == "low_ollie";
            max_air_height = s3::rs::max(max_air_height, s3::rs::unwrap(camera.frame).position[1]);
            CHECK_MSG(skater.player_input.physical.air.flag_441 == 0,
                      "ordinary flicks must not require body-flip mode");
        }
    }
    CHECK_MSG(saw_air, "flick did not launch");
    CHECK_MSG(saw_air_shot, "flick stayed in the chase shot");
    CHECK_MSG(max_air_height < 2.8f, "camera chased the board upward");
    CHECK_MSG(camera.selected_shot() == "bl_high_chase", "normal camera must resume after landing");
    std::fprintf(stderr, "flick=[%d, %d], max camera height=%s\n", flick[0], flick[1],
                 s3::game::fmt::display_f32(max_air_height).c_str());
}

// #[ignore = "requires private stock graphs and animation assets"]
S3_TEST(tricks_select_air_camera_without_chasing_the_board_upward) {
    if (std::getenv("S3_RUN_IGNORED") == nullptr) return;
    for (const auto flick : {std::array<std::int16_t, 2>{0, 32767}, std::array<std::int16_t, 2>{-32767, 16384},
                             std::array<std::int16_t, 2>{32767, 16384}}) {
        replay(flick);
    }
}
