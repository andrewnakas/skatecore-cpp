// Port of skate-game/src/tests/offboard_air_playback.rs (mounted in physics.rs as offboard_air_tests).
//! Production controller/graph/solver regression across an authored walking edge.
// `edge_world` lives in physics_offboard_test_helpers.hpp (shared with the midair/recall tests).
// `#[ignore = "requires private stock animation banks and collections"]`: runs on the cleanroom pack,
// skips without it.
#include <optional>

#include "physics_offboard_test_helpers.hpp"

using namespace s3test::game::offboard_playback;
namespace ground = s3::game::physics::ground;

// #[ignore = "requires private stock animation banks and collections"]
S3_TEST(offboard_walk_off_edge_runs_ground_air_and_landing) {
    auto loaded = Playback::load_with("normal", false, [](GamePhysics& physics) {
        physics.world = edge_world(physics.settings.floor_material, 2.0f, ground::FLOOR_HEIGHT);
    });
    if (!loaded) return;
    auto& r = *loaded;
    auto& skater = r.skater;
    bool grounded = false;
    std::optional<int> air_tick;
    std::optional<int> landed_tick;
    for (int tick = 0; tick < 1200; ++tick) {
        auto actions = r.sample_for_physics(xbox(tick == 20 ? std::uint16_t(0x8000) : std::uint16_t(0), {0, 0},
                                                 tick >= 120 && tick < 480 ? std::array<std::int16_t, 2>{0, 24000}
                                                                           : std::array<std::int16_t, 2>{0, 0},
                                                 {0, 0}));
        const auto advanced = r.advance(actions);
        CHECK_MSG(advanced.is_ok(), "Offboard edge tick" << tick << ": " << advanced.unwrap_err());
        const auto state = skater.player_state.current();
        if (state == PhysicalStateId::BipedGround) {
            grounded = true;
            if (air_tick && !landed_tick) landed_tick = tick;
        }
        if (state == PhysicalStateId::BipedAir) {
            CHECK_MSG(grounded, "Air entered before walking Ground at tick" << tick);
            if (!air_tick) air_tick = tick;
            CHECK_MSG(skater.player_input.physical.off_board.trajectory_valid_331 == 1,
                      "BipedAir Fill did not publish its trajectory at tick" << tick);
            CHECK_MSG(skater.player_input.physical.off_board.flag_304 == 0,
                      "BipedAir invented an object-grab gate at tick" << tick);
        }
        CHECK_MSG(render_pose_finite(skater), "Invalid offboard pose at tick" << tick);
        CHECK_MSG(camera_position_finite(r.camera), "Invalid offboard camera at tick" << tick);
    }
    CHECK_MSG(air_tick.has_value(), "Walking off the authored platform never entered BipedAir");
    CHECK_MSG(landed_tick.has_value(), "BipedAir never returned to BipedGround on the lower floor");
}
