// Port of skate-game/src/tests/manual_playback.rs (mounted in physics.rs as manual_tests).
//! Raw controller -> authored manual states -> real shared solve and release.
// `#[ignore = "requires private stock graph/animation/physics assets"]`: runs on the cleanroom pack,
// skips without it.
#include <cstdio>

#include "physics_offboard_test_helpers.hpp"

using namespace s3test::game::offboard_playback;

namespace {

void run_manual(std::int16_t sign) {
    auto loaded = Playback::load("normal", true);
    if (!loaded) return;
    auto& r = *loaded;
    auto& physics = r.physics;
    auto& skater = r.skater;
    auto& controls = r.controls;
    auto& camera = r.camera;
    bool manual_seen = false;
    bool balance_seen = false;
    bool controller_seen = false;
    bool timer_seen = false;
    bool release_seen = false;
    for (std::uint64_t tick = 0; tick < 330; ++tick) {
        // A sustained partial stick is not a Flickit flick. Use the actual
        // converter and gesture pipeline, including its authored exclusions.
        const bool holding = tick >= 45 && tick < 195;
        auto actions = r.sample_update(
            xbox(tick >= 12 && tick < 40 ? std::uint16_t(0x1000) : std::uint16_t(0), {0, 0}, {0, 0},
                 holding ? std::array<std::int16_t, 2>{0, static_cast<std::int16_t>(sign * 22000)}
                         : std::array<std::int16_t, 2>{0, 0}),
            true);
        const auto advanced = r.advance(actions);
        CHECK_MSG(advanced.is_ok(), "Manual sign=" << sign << " tick=" << tick << ": " << advanced.unwrap_err()
                                                   << "; balance=" << skater.animation_input.fields.balance);
        const auto& motion = skater.animation.motion;
        manual_seen = manual_seen || motion.flags.manualing;
        balance_seen = balance_seen || (motion.flags.manualing && skater.animation_input.fields.balance != 0.0f);
        controller_seen = controller_seen ||
                          (skater.ground.manual.elapsed > 0.0f && skater.ground.manual.angular_correction != 0.0f &&
                           skater.player_state.current() == PhysicalStateId::PhysicsGround);
        if (tick >= 195) {
            CHECK(controls.action_intents.get("Manual") == nullptr);
            CHECK(motion.animation.motion_intents.get("Manual") == nullptr);
            timer_seen = timer_seen || motion.riding.manual_out_timer > 0.0f;
            release_seen = release_seen || (!motion.flags.manualing && skater.animation_input.fields.balance == 0.0f &&
                                            skater.ground.manual.elapsed == 0.0f);
        }
        CHECK_EQ(skater.pose_generation, physics.ticks);
        CHECK(camera.frame.has_value());
        const auto* output = physics.exchange.output();
        CHECK_MSG(output != nullptr, "called `Option::unwrap()` on a `None` value");
        CHECK_EQ(output->tick + 1, physics.ticks);
    }
    CHECK_MSG(manual_seen, "Authored manual never entered: sign=" << sign);
    CHECK_MSG(balance_seen, "Manual never published physical balance: sign=" << sign);
    CHECK_MSG(controller_seen, "Manual never reached Ground controller: sign=" << sign);
    CHECK_MSG(timer_seen, "Manual exit never armed authored timer: sign=" << sign);
    CHECK_MSG(release_seen, "Manual controller did not clear after release: sign=" << sign);
    CHECK_EQ(skater.animation.motion.riding.manual_out_timer, 0.0f);
    std::fprintf(stderr,
                 "Manual sign=%d: authored entry, physical balance/controller, release, pose and camera passed %llu "
                 "ticks\n",
                 sign, static_cast<unsigned long long>(physics.ticks));
}

}  // namespace

// #[ignore = "requires private stock graph/animation/physics assets"]
S3_TEST(raw_controller_manuals_enter_balance_and_release_in_gameplay) {
    for (const std::int16_t sign : {std::int16_t(-1), std::int16_t(1)}) run_manual(sign);
}
