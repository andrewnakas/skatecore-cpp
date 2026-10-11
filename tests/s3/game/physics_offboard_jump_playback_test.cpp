// Port of skate-game/src/tests/offboard_jump_playback.rs (mounted in physics.rs as
// offboard_jump_playback_tests).
//! Raw X -> stock OB_Jump -> Ground launch -> real Air/landing regression.
//! Source: input8259C260, attribute82BDBA88, Ground82D32338/82D32D38,
//! launch82D7BA78. Acceptance bounds are not native tuning/parity fixtures.
// `#[ignore = ...]` private-asset tests: run on the cleanroom pack, skip without it. The panic
// message on a failed tick prints a reduced field set (no Debug formatter for the whole structs).
#include <cmath>
#include <cstdio>
#include <limits>
#include <optional>

#include "physics_offboard_test_helpers.hpp"

using namespace s3test::game::offboard_playback;

namespace {

void assert_released(const GamePhysics& physics, const SkaterRuntime& skater, int tick) {
    const auto state = skater.skateboard_controller.fields.state_448;
    CHECK_MSG(state == 2 || state == 3, "Raw RT did not leave the board free at tick" << tick << ": " << state);
    CHECK_MSG(skater.board_possession.state.selected_hand_424 == 2,
              "Free-board jump retained a selected hand at tick" << tick);
    const std::array<std::uint32_t, 8> released{0, 0, 0, 2, 0, 0, 0, 2};
    CHECK_MSG(physics.board.hook().drive.dynamics == released, "Free-board jump rearmed the deck drive at tick" << tick);
    for (const auto& hand : skater.board_possession.state.hands) {
        CHECK_MSG(hand_dynamics_released(hand.dynamics), "Free-board jump rearmed a hand drive at tick" << tick);
    }
}

void assert_joints(const SkaterRuntime& skater, int tick) {
    if (const auto failure = joint_gap_failure(skater)) {
        //Visible-failure acceptance bound shared with the dismount regression.
        CHECK_MSG(false, "Jump joint failure tick" << tick << ": " << failure->parent << "->" << failure->child
                                                   << " gap=" << failure->gap);
    }
}

void jump_playback_with_tweak(bool throw_board, bool tweak) {
    auto loaded = Playback::load("normal", false);
    if (!loaded) return;
    auto& r = *loaded;
    auto& physics = r.physics;
    auto& skater = r.skater;
    int jump_intents = 0;
    std::optional<int> launch_tick;
    std::optional<int> air_tick;
    std::optional<int> landing_tick;
    int air_frames = 0;
    int landed_frames = 0;
    std::optional<float> launch_height;
    float maximum_height = -std::numeric_limits<float>::infinity();
    bool saw_upward_physical_velocity = false;
    bool saw_completed_selector = false;
    bool saw_stock_end_com_adjustment = false;
    std::array<bool, 4> tweak_directions{false, false, false, false};
    for (int tick = 0; tick < 1200; ++tick) {
        const auto before_state = skater.player_state.current();
        if (tick == 400) {
            CHECK_MSG(before_state == PhysicalStateId::BipedGround, "Raw Y did not settle into Ground before the jump");
            CHECK_MSG((skater.biped_ground.contact.flags_176 & 1u) != 0, "Jump fixture has no actual analyzer support");
            CHECK(skater.skateboard_controller.fields.system_on_452);
            if (throw_board) {
                assert_released(physics, skater, tick);
            } else {
                CHECK_MSG(skater.skateboard_controller.fields.state_448 == 1,
                          "Held variant must begin carrying the board through the real owner");
            }
            launch_height = skater.skeleton.record.centre_of_mass[1];
        }
        //Stock input.cfg: GP_XFace=X=Button14, native action78 -> bit23.
        //Hold X for sixty frames:8259C260 emits only on the rising edge.
        // Release before native82BBB030's <0.1s landing boundary. Holding a
        // tweak into that boundary issues a separate native bit16 request.
        std::optional<std::size_t> tweak_index;
        if (tweak && tick >= 410 && tick < 442) tweak_index = static_cast<std::size_t>((tick - 410) / 8);
        static constexpr std::array<std::array<std::int16_t, 2>, 4> DIRECTIONS{
            {{24000, 0}, {-24000, 0}, {0, 24000}, {0, -24000}}};
        const std::array<std::int16_t, 2> right =
            tweak_index ? DIRECTIONS[*tweak_index] : std::array<std::int16_t, 2>{0, 0};
        std::uint16_t buttons = 0;
        if (tick == 20) {
            buttons = 0x8000;
        } else if (tick >= 400 && tick < 460) {
            buttons = 0x4000;
        }
        //8259C260 publishes throw/retrieve on a full RT rising edge. The
        //stock graph and actual possession owner decide the release.
        const std::array<std::uint8_t, 2> triggers =
            throw_board && tick >= 200 && tick < 260 ? std::array<std::uint8_t, 2>{0, 255}
                                                     : std::array<std::uint8_t, 2>{0, 0};
        auto actions = r.sample_for_physics(xbox(buttons, triggers, {0, 0}, right));
        const auto advanced = r.advance(actions);
        if (advanced.is_err()) {
            const auto& p = skater.player_input.processed;
            CHECK_MSG(false, "Offboard jump tick" << tick << ", thrown=" << throw_board
                                                  << ", prior=" << static_cast<unsigned>(before_state) << ": "
                                                  << advanced.unwrap_err() << "; states=" << p.state_2504 << "/"
                                                  << p.state_2508 << " categories=" << p.category_2516 << "/"
                                                  << p.category_2512 << std::hex << " flags=" << p.flags_2472 << "/"
                                                  << p.flags_2476 << "/" << p.flags_2480 << "/" << p.flags_2484);
        }
        const auto state = skater.player_state.current();
        const auto& p = skater.player_input.processed;
        if (skater.animation.motion.action_intents.contains_key("OB_Jump")) {
            jump_intents += 1;
            CHECK_MSG(tick == 400, "Jump intent was not the raw X rising edge");
        }
        if (tick >= 400) {
            if (throw_board) assert_released(physics, skater, tick);
            const auto com = skater.skeleton.record.centre_of_mass;
            const auto velocity = skater.skeleton.record.centre_of_mass_velocity;
            maximum_height = s3::rs::max(maximum_height, com[1]);
            saw_upward_physical_velocity = saw_upward_physical_velocity || velocity[1] > 0.1f;
            bool finite = true;
            for (float v : velocity) finite = finite && std::isfinite(v);
            CHECK_MSG(finite, "Nonfinite jump velocity tick" << tick);
            assert_joints(skater, tick);
            if (state == PhysicalStateId::BipedGround && (p.flags_2476 & 0x80000u) != 0 &&
                skater.biped_ground.ground.flags_144_to_150[2]) {
                if (!launch_tick) launch_tick = tick;
                CHECK_MSG((skater.biped_ground.contact.flags_176 & 1u) != 0,
                          "Expected an explicit supported jump, not falling off support");
                const auto& output = skater.player_input.physical.off_board;
                CHECK_MSG(output.flag_328 == 1, "Ground Fill dropped the launch flag");
                CHECK_MSG(output.scalar_32 == 1.0f, "Ground Fill dropped the selector scalar");
                const auto& selector = skater.offboard_air_selector.core;
                CHECK_MSG(selector.sampling.pending_8492, "Ground did not submit its real Air query");
                CHECK_EQ(selector.launch.kind_108, 6u);
                CHECK_MSG(selector.launch.kind_112 == 3, "Launch was not native jump mode4");
                CHECK_MSG(selector.launch.velocity_0[1] > 0.0f, "Jump packet has no upward velocity");
            }
            if (state == PhysicalStateId::BipedAir) {
                const auto end_com = skater.animation_input.fields.animation_end_com;
                const auto translation = skater.animation_input.fields.animation_translation;
                if (end_com[1] > 0.1f && translation[1] <= 0.1f && skater.biped_air.state.flags_544_550[0]) {
                    //Native82D2EFF0/F014 reads0xB40, not0xB30. The stock
                    //jump's distinct attributes make a swapped callsite fail.
                    CHECK_MSG(skater.biped_air.state.flags_544_550[1],
                              "Air did not consume stock AnimEndCOM at tick" << tick);
                    saw_stock_end_com_adjustment = true;
                }
                if (tweak_index) {
                    const auto& animation = skater.animation.motion.animation;
                    if (animation.channels.has("AirBodyTweak")) {
                        CHECK(animation.motion_intents.contains_key("OB_AirBodyTweakX"));
                        CHECK(animation.motion_intents.contains_key("OB_AirBodyTweakY"));
                        tweak_directions[*tweak_index] = true;
                    }
                }
                CHECK_MSG(launch_tick.has_value(), "Air preceded the completed Ground jump packet");
                CHECK_MSG(!landing_tick.has_value(), "A held X/republication caused another Air entry");
                if (!air_tick) air_tick = tick;
                air_frames += 1;
                const auto& selector = skater.offboard_air_selector.core;
                saw_completed_selector =
                    saw_completed_selector || (!selector.sampling.pending_8492 && selector.selected_index.has_value());
                CHECK_MSG(skater.player_input.physical.off_board.trajectory_valid_331 == 1,
                          "Air Fill has no real trajectory tick" << tick);
            }
            if (air_tick && state == PhysicalStateId::BipedGround) {
                if (!landing_tick) landing_tick = tick;
                landed_frames += 1;
            }
            if (state != before_state || launch_tick == tick) {
                std::fprintf(stderr,
                             "Jump tick%d: %u->%u COM=[%g, %g, %g, %g] velocity=[%g, %g, %g, %g] "
                             "flags=%08x/%08x/%08x support=%x remaining=%g launch=%d\n",
                             tick, unsigned(before_state), unsigned(state), double(com[0]), double(com[1]),
                             double(com[2]), double(com[3]), double(velocity[0]), double(velocity[1]),
                             double(velocity[2]), double(velocity[3]), unsigned(p.flags_2476), unsigned(p.flags_2480),
                             unsigned(p.flags_2484), unsigned(skater.biped_ground.contact.flags_176),
                             double(skater.biped_air.state.time_remaining_444), launch_tick ? *launch_tick : -1);
            }
        }
        CHECK_MSG(render_pose_finite(skater), "Nonfinite jump pose tick" << tick);
        CHECK_MSG(camera_position_finite(r.camera), "Nonfinite jump camera tick" << tick);
        if (landed_frames >= 120) break;
    }
    CHECK_MSG(jump_intents == 1, "A single raw X edge must produce one AG jump intent (" << jump_intents << ")");
    if (tweak) {
        CHECK_MSG(tweak_directions[0] && tweak_directions[1] && tweak_directions[2] && tweak_directions[3],
                  "Raw right-stick directions did not all reach the live AirBodyTweak channel: ["
                      << tweak_directions[0] << ", " << tweak_directions[1] << ", " << tweak_directions[2] << ", "
                      << tweak_directions[3] << "]");
    }
    CHECK_MSG(launch_tick.has_value(), "Stock jump animation never produced a Ground launch");
    CHECK_MSG(air_frames >= 8, "Jump did not sustain an airborne interval: " << air_frames);
    CHECK_MSG(saw_completed_selector, "Real jump query was never consumed");
    CHECK_MSG(saw_stock_end_com_adjustment, "Jump did not exercise the distinct stock AnimEndCOM/AnimTrans inputs");
    CHECK_MSG(saw_upward_physical_velocity, "Jump did not raise the physical skeleton");
    CHECK_MSG(launch_height.has_value(), "jump attempted");
    CHECK_MSG(maximum_height > *launch_height + 0.1f, "Physical COM never rose above the supported launch height");
    CHECK_MSG(landing_tick.has_value() && landed_frames >= 120,
              "Jump did not return to sustained Ground through the native selector");
}

void jump_playback(bool throw_board) { jump_playback_with_tweak(throw_board, false); }

}  // namespace

// #[ignore = "requires private stock animation banks and collections"]
S3_TEST(raw_x_offboard_jump_connects_input_ground_launch_air_and_landing) { jump_playback(false); }

// #[ignore = "requires private stock animation banks and collections"]
S3_TEST(raw_x_offboard_jump_with_thrown_board_connects_ground_air_and_landing) { jump_playback(true); }

// #[ignore = "requires private stock assets; connected raw right-stick gameplay regression"]
S3_TEST(raw_right_stick_four_directions_reach_air_tweak_with_held_board) { jump_playback_with_tweak(false, true); }

// #[ignore = "requires private stock assets; connected raw right-stick gameplay regression"]
S3_TEST(raw_right_stick_four_directions_reach_air_tweak_with_released_board) { jump_playback_with_tweak(true, true); }
