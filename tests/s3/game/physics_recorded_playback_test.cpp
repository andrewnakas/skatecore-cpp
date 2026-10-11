// Port of skate-game/src/tests/recorded_playback.rs (mounted in physics.rs as recorded_tests).
//! User-captured packets through the production frame. The level, initial
//! pose and controller phase are clone-owned; this is not a stock parity oracle.
// `#[ignore = "requires private assets and SKATE3_INPUT_RECORDING"]`: runs on the cleanroom pack when
// SKATE3_INPUT_RECORDING names a recorder JSONL file on disk; skips otherwise (the recording is not
// part of the pack).
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <tuple>

#include "physics_offboard_test_helpers.hpp"

using namespace s3test::game::offboard_playback;

// #[ignore = "requires private assets and SKATE3_INPUT_RECORDING"]
S3_TEST(recorded_controller_sequence_runs_production_gameplay) {
    const char* path = std::getenv("SKATE3_INPUT_RECORDING");
    if (path == nullptr) {
        std::printf("  (skip: set SKATE3_INPUT_RECORDING)\n");
        return;
    }
    std::ifstream file(path);
    CHECK_MSG(static_cast<bool>(file), "cannot read " << path);
    std::stringstream text;
    text << file.rdbuf();
    auto parsed = s3::data::Recording::read(text.str());
    CHECK_MSG(parsed.ok, parsed.error);
    const auto& recording = parsed.value;
    // User confirmed this recording was made on flat ground. Keep the same
    // authored surface material and query metadata while removing the ramp.
    auto loaded = Playback::load_with("normal", true, [](GamePhysics& physics) {
        physics.world = s3::game::physics::ground::flat_world(physics.world.triangles()[0].material);
    });
    if (!loaded) return;
    auto& r = *loaded;
    auto& physics = r.physics;
    auto& skater = r.skater;
    auto& camera = r.camera;
    double seconds = 0.0;
    using Key = std::tuple<PhysicalStateId, bool, bool, std::optional<std::string>, std::uint32_t>;
    std::optional<Key> previous;
    std::uint32_t airborne = 0;
    std::uint32_t manual = 0;
    std::uint32_t bail = 0;
    float maximum_rider_com_y = -std::numeric_limits<float>::infinity();
    while (seconds * 1000000.0 <= static_cast<double>(recording.duration_us())) {
        const auto& sample = recording.at_elapsed_us(static_cast<std::uint64_t>(seconds * 1000000.0));
        CHECK_MSG(sample.connected, "fixture disconnected at " << sample.t_us);
        const auto raw = sample.raw_state();
        auto actions = r.sample_update(xbox(raw.buttons, raw.triggers, raw.left, raw.right), true);
        const float dt = physics.settings.step.simulation.time_step;
        const auto result = r.advance(actions);
        const auto state = skater.player_state.current();
        const auto& motion = skater.animation.motion;
        Key current{state, motion.flags.manualing, motion.flags.doing_trick, motion.animation.current_name,
                    skater.wipeout.state.count};
        if (!previous || *previous != current || result.is_err()) {
            const auto& deck = physics.board.bodies()[6].rates;
            const auto& com = skater.skeleton.record.centre_of_mass;
            std::fprintf(stderr,
                         "RECORDING t=%.4f sample=%llu state=%u manual=%d trick=%d anim=%s wipeouts=%u p=[%g, %g, %g] "
                         "v=[%g, %g, %g] balance=%g jump=%g rider_com=[%g, %g, %g]\n",
                         seconds, static_cast<unsigned long long>(sample.sample), static_cast<unsigned>(state),
                         int(motion.flags.manualing), int(motion.flags.doing_trick),
                         motion.animation.current_name ? motion.animation.current_name->c_str() : "None",
                         skater.wipeout.state.count, double(deck.position.x), double(deck.position.y),
                         double(deck.position.z), double(deck.linear_velocity.x), double(deck.linear_velocity.y),
                         double(deck.linear_velocity.z), double(skater.animation_input.fields.balance),
                         double(skater.animation_input.extra.jump_strength), double(com[0]), double(com[1]),
                         double(com[2]));
            previous = current;
        }
        CHECK_MSG(result.is_ok(), "Recording t=" << seconds << " sample=" << sample.sample << ": "
                                                 << result.unwrap_err());
        airborne += static_cast<std::uint32_t>(s3::player::state::category(state) == 200);
        manual += static_cast<std::uint32_t>(motion.flags.manualing);
        bail += static_cast<std::uint32_t>(s3::player::state::category(state) == 300);
        maximum_rider_com_y = s3::rs::max(maximum_rider_com_y, skater.skeleton.record.centre_of_mass[1]);
        CHECK_EQ(skater.pose_generation, physics.ticks);
        CHECK(camera.frame.has_value());
        seconds += static_cast<double>(dt);
    }
    CHECK_MSG(airborne > 0, "capture never reached an onboard air state");
    std::fprintf(stderr,
                 "RECORDING completed: ticks=%llu air=%u manual=%u bail=%u max_rider_com_y=%g; smoke check only, NOT "
                 "stock parity\n",
                 static_cast<unsigned long long>(physics.ticks), airborne, manual, bail,
                 static_cast<double>(maximum_rider_com_y));
}
