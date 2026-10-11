// Port of skate-game/src/tests/offboard_recall_playback.rs (mounted in physics.rs as
// offboard_recall_playback_tests).
//! Real raw controller -> stock graphs -> possession -> shared solver checks.
//! Acceptance regressions, not original-game or Xenon parity fixtures.
// `#[ignore = "requires private stock animation banks and collections"]`: runs on the cleanroom pack,
// skips without it.
// Not ported verbatim (diagnostic output only, never assertions): the SKATE3_TRACE_OFFBOARD_ANIMATION
// bone dumps (evaluator hierarchy/bone names), the stock B_OBAIR_BODYTWEAK metadata walk, and the
// Debug dumps of whole structs; the per-tick "board replay" and APEX/RTY prints keep scalar fields.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

#include "physics_offboard_test_helpers.hpp"
#include "s3/game/graph_host/motion_stock_conditions.hpp"

using namespace s3test::game::offboard_playback;
namespace ground = s3::game::physics::ground;
using s3::player::state::category;

namespace {

bool trace_animation() { return std::getenv("SKATE3_TRACE_OFFBOARD_ANIMATION") != nullptr; }

std::string opt_f32(const float* v) { return v ? std::to_string(*v) : std::string("None"); }

/// Rust `struct Replay` (a Playback with the stock PHYSICS_MODE skater and default controls).
struct Replay {
    std::unique_ptr<Playback> r;

    static std::optional<Replay> load() {
        auto loaded = Playback::load(PHYSICS_MODE, false);
        if (!loaded) return std::nullopt;
        return Replay{std::move(loaded)};
    }
    SkaterRuntime& skater() { return r->skater; }
    GamePhysics& physics() { return r->physics; }

    void tick(std::size_t tick, std::uint16_t buttons, std::array<std::uint8_t, 2> triggers) {
        tick_raw(tick, xbox(buttons, triggers, {0, 0}, {0, 0}));
    }
    void tick_raw(std::size_t tick, const XboxState& raw) {
        auto& s = r->skater;
        const auto previous_state = s.player_state.current();
        const auto previous_board = s.skateboard_controller.fields.state_448;
        auto actions = r->sample_for_physics(raw);
        const auto advanced = r->advance(actions);
        CHECK_MSG(advanced.is_ok(), "Recall/remount tick" << tick << ": " << advanced.unwrap_err());
        const auto current_state = s.player_state.current();
        const auto current_board = s.skateboard_controller.fields.state_448;
        if (previous_state != current_state || previous_board != current_board || tick % 240 == 0 ||
            (tick >= 780 && tick < 900 && tick % 10 == 0)) {
            const auto& channel = s.animation.motion.animation.channels;
            std::fprintf(stderr,
                         "board replay tick%zu: (%u, %u)->(%u, %u) flags=%08x/%08x progress=%g physical=%s "
                         "recallAG=%d mountMG=%d channel=%d/%g/%g\n",
                         tick, unsigned(previous_state), unsigned(previous_board), unsigned(current_state),
                         unsigned(current_board), unsigned(s.player_input.processed.flags_2476),
                         unsigned(s.player_input.processed.flags_2480),
                         double(s.board_possession.state.retrieval.progress_200),
                         s.animation.motion.toggle_board_physical ? "Some" : "None",
                         int(s.animation.motion.action_intents.contains_key("OB_RetrieveBoard")),
                         int(s.animation.motion.animation.motion_intents.contains_key("OB_Mount")),
                         int(channel.has("RetrieveBoard")), double(channel.elapsed("RetrieveBoard")),
                         double(channel.remaining("RetrieveBoard")));
        }
        CHECK_MSG(render_pose_finite(s), "Nonfinite pose tick" << tick);
        bool roots_finite = true;
        for (const auto& row : s.animated_skeleton.roots.animation_to_world)
            for (float v : row) roots_finite = roots_finite && std::isfinite(v);
        CHECK_MSG(roots_finite, "Nonfinite FULL animation root (including W) tick" << tick);
        bool com_finite = true;
        for (float v : s.skeleton.record.centre_of_mass) com_finite = com_finite && std::isfinite(v);
        for (float v : s.skeleton.record.centre_of_mass_velocity) com_finite = com_finite && std::isfinite(v);
        CHECK_MSG(com_finite, "Nonfinite full physical COM/velocity tick" << tick);
        CHECK_MSG(camera_position_finite(r->camera), "Nonfinite camera tick" << tick);
    }
    void assert_held() const {
        const auto& s = r->skater;
        CHECK_EQ(s.skateboard_controller.fields.state_448, 1u);
        CHECK(s.skateboard_controller.fields.system_on_452);
        const auto hand = static_cast<std::size_t>(s.board_possession.state.selected_hand_424);
        CHECK_MSG(hand < 2, "Held board has no physical hand owner");
        CHECK(!hand_dynamics_released(s.board_possession.state.hands[hand].dynamics));
        CHECK(s.board_possession_live.volumes.deck);
        CHECK_EQ(s.player_input.physical.off_board.flag_311, 1u);
    }
};

void assert_joint_anchors(const SkaterRuntime& skater, std::size_t tick) {
    if (const auto failure = joint_gap_failure(skater)) {
        //Visible-failure bound already used by the raw dismount/jump tests;
        //this is acceptance measurement, never a solver clamp or parity claim.
        CHECK_MSG(false, "Combined replay joint failure tick" << tick << ": " << failure->parent << "->"
                                                              << failure->child << " gap=" << failure->gap);
    }
}

std::optional<float> intent(const s3::graph::intents::IntentMap& map, const char* name) {
    const float* v = map.get(name);
    return v ? std::optional<float>(*v) : std::nullopt;
}

}  // namespace

// #[ignore = "requires private stock animation banks and collections"]
S3_TEST(raw_right_bumper_reaches_offboard_stock_arm_ready_channel) {
    auto loaded = Replay::load();
    if (!loaded) return;
    auto& r = *loaded;
    bool ready = false;
    for (std::size_t tick = 0; tick < 300; ++tick) {
        const bool held = tick >= 200 && tick < 230;
        r.tick(tick, tick == 20 ? std::uint16_t(0x8000) : held ? std::uint16_t(0x0200) : std::uint16_t(0), {0, 0});
        if (trace_animation() && (tick == 199 || tick == 220 || tick == 260)) {
            std::fprintf(stderr, "RB_ARM tick=%zu selected_hand=%u\n", tick,
                         unsigned(r.skater().board_possession.state.selected_hand_424));
        }
        if (tick >= 200) {
            CHECK_EQ(r.skater().player_state.current(), PhysicalStateId::BipedGround);
            const std::optional<float> expected = held ? std::optional<float>(1.0f) : std::nullopt;
            CHECK_MSG(intent(r.r->controls.action_intents, "GrabWorld") == expected,
                      "RB AG publication at tick" << tick);
            CHECK_MSG(intent(r.skater().animation.motion.animation.motion_intents, "GrabWorld") == expected,
                      "Stock AG-to-MG publication at tick" << tick);
            if (held) {
                ready = ready || r.skater().animation.motion.animation.channels.has("SkitchAntic");
                CHECK_MSG((r.skater().player_input.processed.flags_2476 & 0x400000u) != 0,
                          "Stock offboard AttachIntent did not publish GrabWorld");
            }
        }
    }
    CHECK_MSG(ready, "Stock Shove never started the offboard arm-ready channel");
    CHECK_MSG(!r.skater().animation.motion.animation.channels.has("SkitchAntic"),
              "Arm-ready channel persisted after RB release and its fade");
}

// #[ignore = "requires private stock animation banks and collections"]
S3_TEST(connected_biped_ground_advances_shared_board_steering) {
    auto loaded = Replay::load();
    if (!loaded) return;
    auto& r = *loaded;
    for (std::size_t tick = 0; tick < 200; ++tick) {
        r.tick(tick, tick == 20 ? std::uint16_t(0x8000) : std::uint16_t(0), {0, 0});
    }
    CHECK_EQ(r.skater().player_state.current(), PhysicalStateId::BipedGround);
    r.skater().ground.steering.deck_tilt = 0.75f;
    r.skater().ground.steering.targets = {0.25f, -0.5f};
    r.skater().ground.steering.activation_time = {0.1f, 0.1f};
    auto expected = r.skater().ground.steering;
    r.tick(200, 0, {0, 0});
    const auto& p = r.skater().player_input.processed;
    expected.update(0.0f, r.skater().air_settings.steering_blend, p.flags_2468, p.flags_2472);
    CHECK_MSG(r.skater().ground.steering == expected,
              "Ground must call the existing board steering owner once after feet, before possession");
}

// #[ignore = "requires private stock animation banks and collections"]
S3_TEST(diagnostic_released_board_jump_apex_rb_arm_response) {
    // Diagnostic comparison, not a backwards-man compatibility pass. No Y
    // remount is injected yet: isolate the user's reported RB arm response.
    auto first = Replay::load();
    if (!first) return;
    auto second = Replay::load();
    if (!second) return;
    std::array<Replay*, 2> runs{&*first, &*second};
    std::optional<std::size_t> apex_tick;
    bool rose = false;
    for (std::size_t tick = 0; tick < 560; ++tick) {
        {
            const auto& s = runs[0]->skater();
            if (s.player_state.current() == PhysicalStateId::BipedAir && tick > 400) {
                const float vy = s.skeleton.record.centre_of_mass_velocity[1];
                rose = rose || vy > 0.0f;
                if (rose && vy <= 0.0f && !apex_tick) apex_tick = tick;
            }
        }
        for (std::size_t variant = 0; variant < runs.size(); ++variant) {
            auto& r = *runs[variant];
            const bool rb = variant == 1 && apex_tick == tick;
            std::uint16_t buttons = 0;
            if (tick == 20) {
                buttons = 0x8000;
            } else if (tick == 400) {
                buttons = 0x4000;
            } else if (rb) {
                buttons = 0x0200;
            }
            r.tick(tick, buttons,
                   tick >= 200 && tick < 260 ? std::array<std::uint8_t, 2>{0, 255} : std::array<std::uint8_t, 2>{0, 0});
            if (tick == 400) {
                const auto board = r.skater().skateboard_controller.fields.state_448;
                CHECK_MSG(board == 2 || board == 3, "Diagnostic did not release the board before jumping");
            }
            if (apex_tick && tick >= *apex_tick && tick <= *apex_tick + 10) {
                const auto& s = r.skater();
                std::fprintf(stderr,
                             "APEX_RB variant=%zu tick=%zu apex=%zu state=%u AG=%s MG=%s flags=%08x/%08x/%08x "
                             "board=%u\n",
                             variant, tick, *apex_tick, unsigned(s.player_state.current()),
                             opt_f32(r.r->controls.action_intents.get("GrabWorld")).c_str(),
                             opt_f32(s.animation.motion.animation.motion_intents.get("GrabWorld")).c_str(),
                             unsigned(s.player_input.processed.flags_2476),
                             unsigned(s.player_input.processed.flags_2480),
                             unsigned(s.player_input.processed.flags_2484),
                             unsigned(s.skateboard_controller.fields.state_448));
            }
        }
    }
    CHECK_MSG(apex_tick.has_value(), "Diagnostic did not reach a physical jump apex");
}

// #[ignore = "requires private stock animation banks and collections"]
S3_TEST(raw_repeated_walk_turn_throw_recall_jump_remount_preserves_full_roots_and_joints) {
    auto loaded = Replay::load();
    if (!loaded) return;
    auto& r = *loaded;
    // The uninterrupted route crosses the default flat fixture's z=50 edge
    // at tick4585, before cycle3's commanded jump. Give this endurance test
    // actual collision geometry covering its route; keep the dedicated ledge
    // tests and every landing/ownership/root/joint assertion unchanged.
    r.physics().world = edge_world(r.physics().settings.floor_material, 250.0f, ground::HEIGHT - 100.0f);
    //Three uninterrupted cycles exceed both reported827 and3315 histories.
    //All timing and movement below are test commands, not physics parameters.
    for (std::size_t cycle = 0; cycle < 3; ++cycle) {
        bool dismounted = false;
        bool released = false;
        bool recall_pulse = false;
        bool returning = false;
        bool caught = false;
        int air_frames = 0;
        int landed_frames = 0;
        bool mounted = false;
        for (std::size_t local = 0; local < 1800; ++local) {
            const std::size_t tick = cycle * 1800 + local;
            const std::int16_t side = cycle % 2 == 0 ? 20000 : -20000;
            const std::int16_t neg = static_cast<std::int16_t>(-side);
            std::array<std::int16_t, 2> left{0, 0};
            if (local >= 120 && local < 220) {
                left = {0, 24000};
            } else if (local >= 220 && local < 280) {
                left = {side, 0};
            } else if (local >= 280 && local < 320) {
                left = {neg, 12000};
            } else if (local >= 480 && local < 540) {
                left = {side, 18000};
            } else if (local >= 540 && local < 600) {
                left = {neg, 0};
            } else if (local >= 930 && local < 1030) {
                left = {0, 18000};
            } else if (local >= 1030 && local < 1120) {
                left = {side, 12000};
            } else if (local >= 1160 && local < 1220) {
                left = {neg, 8000};
            }
            std::uint16_t buttons = 0;
            if (local == 20 || local == 1400) {
                buttons = 0x8000;
            } else if (local >= 1050 && local < 1110) {
                buttons = 0x4000;
            }
            const bool rt = (local >= 400 && local < 460) || (local >= 800 && local < 860);
            r.tick_raw(tick, xbox(buttons, rt ? std::array<std::uint8_t, 2>{0, 255} : std::array<std::uint8_t, 2>{0, 0},
                                  left, {0, 0}));
            const auto& s = r.skater();
            const auto state = s.player_state.current();
            const auto controller = s.skateboard_controller.fields;
            dismounted = dismounted || (local < 400 && state == PhysicalStateId::BipedGround);
            if (local >= 400 && local < 800 && (controller.state_448 == 2 || controller.state_448 == 3)) {
                released = true;
                CHECK_MSG(s.board_possession.state.selected_hand_424 == 2,
                          "Free board retained a hand in cycle" << cycle << " tick" << tick);
                for (const auto& hand : s.board_possession.state.hands) CHECK(hand_dynamics_released(hand.dynamics));
            }
            if (local >= 800 && local < 1050) {
                recall_pulse = recall_pulse || (s.player_input.processed.flags_2476 & 0x800u) != 0;
                if (controller.state_448 == 4) {
                    returning = true;
                    CHECK(!s.board_possession_live.volumes.deck);
                    CHECK(!s.board_possession_live.volumes.trucks);
                    CHECK(!s.board_possession_live.volumes.wheels);
                }
                if (returning && controller.state_448 == 1) {
                    r.assert_held();
                    caught = true;
                }
            }
            if (local >= 1050 && local < 1400) {
                if (state == PhysicalStateId::BipedAir) air_frames += 1;
                if (air_frames > 0 && state == PhysicalStateId::BipedGround) landed_frames += 1;
            }
            if (local >= 1400 && (category(state) == 100 || category(state) == 200)) {
                mounted = true;
                CHECK(!controller.system_on_452);
                CHECK_EQ(controller.state_448, 0u);
                CHECK_EQ(s.board_possession.state.selected_hand_424, 2u);
            }
            if (tick >= 120) assert_joint_anchors(s, tick);
        }
        CHECK_MSG(dismounted && released && recall_pulse && returning && caught,
                  "Incomplete possession cycle" << cycle << ": dismounted=" << dismounted << " free=" << released
                                                << " pulse=" << recall_pulse << " returning=" << returning
                                                << " caught=" << caught);
        CHECK_MSG(air_frames >= 8 && landed_frames >= 60, "Walking/turning jump cycle"
                                                              << cycle << " lacks sustained Air/landing: " << air_frames
                                                              << "/" << landed_frames);
        CHECK_MSG(mounted, "Remount failed after throw/recall/walking jump cycle" << cycle);
        CHECK_MSG(r.skater().player_state.current() == PhysicalStateId::PhysicsGround,
                  "Cycle" << cycle << " did not finish stably onboard before next raw dismount");
    }
}

// #[ignore = "requires private stock animation banks and collections"]
S3_TEST(raw_y_dismount_then_rt_within_six_frames_does_not_bail) {
    // User reproduction: idle onboard, Y, then RT a few frames later. The
    // first unexpected bail is the failure, not only later joint explosion.
    // delay0 is the Y-only control; 1..6 are delayed RT; case7 is simultaneous.
    for (std::size_t delay = 0; delay <= 7; ++delay) {
        std::fprintf(stderr, "DISMOUNT_RT_CASE delay=%zu\n", delay);
        auto loaded = Replay::load();
        if (!loaded) return;
        auto& r = *loaded;
        bool dismounted = false;
        for (std::size_t tick = 0; tick < 240; ++tick) {
            const std::size_t rt = delay == 7 ? 20 : 20 + delay;
            r.tick(tick, tick == 20 ? std::uint16_t(0x8000) : std::uint16_t(0),
                   delay != 0 && tick >= rt && tick < rt + 30 ? std::array<std::uint8_t, 2>{0, 255}
                                                              : std::array<std::uint8_t, 2>{0, 0});
            if (trace_animation() && tick >= 20 && tick <= 24) {
                const auto& s = r.skater();
                const auto& root = s.animated_skeleton.roots.animation_to_world[3];
                std::fprintf(stderr, "YRT_INPUT delay=%zu tick=%zu root=[%g, %g, %g, %g]\n", delay, tick,
                             double(root[0]), double(root[1]), double(root[2]), double(root[3]));
            }
            const auto state = r.skater().player_state.current();
            dismounted = dismounted || category(state) == 500;
            const auto& p = r.skater().player_input.processed;
            CHECK_MSG(category(state) != 300,
                      "Y then RT caused bail: delay=" << delay << " tick=" << tick
                                                      << " state=" << static_cast<unsigned>(state) << " requested="
                                                      << static_cast<unsigned>(r.skater().player_state.requested_state)
                                                      << " board=" << r.skater().skateboard_controller.fields.state_448
                                                      << std::hex << " flags=[" << p.flags_2468 << ", "
                                                      << p.flags_2472 << ", " << p.flags_2476 << ", " << p.flags_2480
                                                      << ", " << p.flags_2484 << "]");
        }
        CHECK_MSG(dismounted, "Y never dismounted in delay=" << delay << " reproduction");
    }
}

// #[ignore = "requires private stock animation banks and collections"]
S3_TEST(diagnostic_rt_before_y_dismount_possession_timing) {
    auto loaded = Replay::load();
    if (!loaded) return;
    auto& r = *loaded;
    for (std::size_t tick = 0; tick < 180; ++tick) {
        r.tick(tick, tick == 21 ? std::uint16_t(0x8000) : std::uint16_t(0),
               tick >= 20 && tick < 50 ? std::array<std::uint8_t, 2>{0, 255} : std::array<std::uint8_t, 2>{0, 0});
        if (tick >= 19 && tick < 65) {
            const auto& s = r.skater();
            const auto& root = s.animated_skeleton.roots.animation_to_world[3];
            const auto& deck = s.animated_skeleton.animation_board[3];
            const auto& channels = s.animation.motion.animation.channels;
            std::fprintf(stderr,
                         "RTY_TIMING tick=%zu state=%u board=%u system_on=%d root=[%g, %g, %g] deck=[%g, %g, %g] "
                         "physical=%s channel=%d/%g/%g flags=%08x/%08x\n",
                         tick, unsigned(s.player_state.current()), unsigned(s.skateboard_controller.fields.state_448),
                         int(s.skateboard_controller.fields.system_on_452), double(root[0]), double(root[1]),
                         double(root[2]), double(deck[0]), double(deck[1]), double(deck[2]),
                         s.animation.motion.toggle_board_physical ? "Some" : "None", int(channels.has("RetrieveBoard")),
                         double(channels.elapsed("RetrieveBoard")), double(channels.remaining("RetrieveBoard")),
                         unsigned(s.player_input.processed.flags_2476), unsigned(s.player_input.processed.flags_2480));
        }
    }
    // Diagnostic completion is not an assertion that possession timing matches stock.
}

// #[ignore = "requires private stock animation banks and collections"]
S3_TEST(raw_deck_free_predicate_tracks_completed_possession_not_not_held) {
    using s3::game::graph_host::motion_stock_conditions::Condition;
    auto loaded = Replay::load();
    if (!loaded) return;
    auto& r = *loaded;
    std::array<bool, 5> seen{};
    const Condition is_deck_free{Condition::IsDeckFree{}};
    for (std::size_t tick = 0; tick < 1000; ++tick) {
        const auto published_state = r.skater().skateboard_controller.fields.state_448;
        const bool rt = (tick >= 400 && tick < 430) || (tick >= 800 && tick < 830);
        r.tick(tick, tick == 20 ? std::uint16_t(0x8000) : std::uint16_t(0),
               rt ? std::array<std::uint8_t, 2>{0, 255} : std::array<std::uint8_t, 2>{0, 0});
        const bool free = is_deck_free.evaluate(r.skater().animation.motion).unwrap();
        CHECK_MSG(free == (published_state == 2 || published_state == 3 || published_state == 4),
                  "IsDeckFree tick" << tick << " completed board state=" << published_state);
        if (static_cast<std::size_t>(published_state) < seen.size()) seen[published_state] = true;
    }
    CHECK_MSG(seen[0] && seen[1] && seen[2] && seen[4], "Replay must cover riding, held, released and returning: ["
                                                            << seen[0] << ", " << seen[1] << ", " << seen[2] << ", "
                                                            << seen[3] << ", " << seen[4] << "]");
}

// #[ignore = "requires private stock animation banks and collections"]
S3_TEST(raw_y_and_right_trigger_adjacent_edges_preserve_roots_and_joints) {
    // Reproduction commands only: exercise overlapping dismount/throw and
    // remount/recall without injecting intentions or physical state changes.
    for (const std::string context : {"onboard", "held", "free"}) {
        for (const long offset : {-1L, 0L, 1L}) {
            std::fprintf(stderr, "Y_RT_CASE context=%s trigger_offset=%ld\n", context.c_str(), offset);
            auto loaded = Replay::load();
            if (!loaded) return;
            auto& r = *loaded;
            for (std::size_t tick = 0; tick < 1000; ++tick) {
                const auto trigger_tick = static_cast<std::size_t>(600L + offset);
                const bool preparing_release = context == "free" && tick >= 250 && tick < 310;
                const std::uint16_t buttons =
                    (context != "onboard" && tick == 20) || tick == 600 ? std::uint16_t(0x8000) : std::uint16_t(0);
                const bool rt = preparing_release || (tick >= trigger_tick && tick < trigger_tick + 30);
                r.tick_raw(tick, xbox(buttons,
                                      rt ? std::array<std::uint8_t, 2>{0, 255} : std::array<std::uint8_t, 2>{0, 0},
                                      tick >= 450 && tick < 750 ? std::array<std::int16_t, 2>{0, 22000}
                                                                : std::array<std::int16_t, 2>{0, 0},
                                      {0, 0}));
                if (tick == 440) {
                    if (context == "onboard") {
                        const auto c = category(r.skater().player_state.current());
                        CHECK(c == 100 || c == 200);
                    } else if (context == "held") {
                        r.assert_held();
                    } else {
                        const auto board = r.skater().skateboard_controller.fields.state_448;
                        CHECK(board == 2 || board == 3);
                    }
                }
                if (tick >= 120) assert_joint_anchors(r.skater(), tick);
            }
        }
    }
}

// #[ignore = "requires private stock animation banks and collections"]
S3_TEST(raw_walk_turn_and_recall_without_jumping_preserves_full_roots_and_joints) {
    auto loaded = Replay::load();
    if (!loaded) return;
    auto& r = *loaded;
    bool released = false;
    bool returning = false;
    bool caught = false;
    for (std::size_t tick = 0; tick < 3600; ++tick) {
        const std::size_t phase = tick % 1200;
        const bool rt = (phase >= 400 && phase < 430) || (phase >= 800 && phase < 830);
        std::array<std::int16_t, 2> left{0, 0};
        if (tick >= 120) {
            if (phase < 300) {
                left = {0, 24000};
            } else if (phase < 600) {
                left = {18000, 14000};
            } else if (phase < 900) {
                left = {-18000, 18000};
            }
        }
        r.tick_raw(tick, xbox(tick == 20 ? std::uint16_t(0x8000) : std::uint16_t(0),
                              rt ? std::array<std::uint8_t, 2>{0, 255} : std::array<std::uint8_t, 2>{0, 0}, left,
                              {0, 0}));
        const auto state = r.skater().skateboard_controller.fields.state_448;
        released = released || state == 2 || state == 3;
        returning = returning || state == 4;
        caught = caught || (returning && state == 1);
        if (tick >= 120) assert_joint_anchors(r.skater(), tick);
    }
    CHECK_MSG(released && returning && caught, "Walking replay did not complete throw/recall");
}

// #[ignore = "requires private stock animation banks and collections"]
S3_TEST(raw_trigger_recall_returns_free_board_to_physical_hand) {
    auto loaded = Replay::load();
    if (!loaded) return;
    auto& r = *loaded;
    bool released = false;
    bool recall_intent = false;
    bool recall_attribute = false;
    bool returning = false;
    bool caught = false;
    for (std::size_t tick = 0; tick < 1800; ++tick) {
        //A second right-trigger rising edge requests recall once the board is
        //free. No injected intentions, animation attributes or physical states.
        const bool rt = (tick >= 400 && tick < 460) || (tick >= 800 && tick < 860);
        r.tick(tick, tick == 20 ? std::uint16_t(0x8000) : std::uint16_t(0),
               rt ? std::array<std::uint8_t, 2>{0, 255} : std::array<std::uint8_t, 2>{0, 0});
        if (tick == 300) r.assert_held();
        const auto& s = r.skater();
        if (tick >= 400 && tick < 800) {
            const auto board = s.skateboard_controller.fields.state_448;
            released = released || board == 2 || board == 3;
        }
        if (tick >= 800) {
            recall_intent = recall_intent || s.animation.motion.action_intents.contains_key("OB_RetrieveBoard");
            recall_attribute = recall_attribute || (s.player_input.processed.flags_2476 & 0x800u) != 0;
            returning = returning || s.skateboard_controller.fields.state_448 == 4;
            if (returning && s.skateboard_controller.fields.state_448 == 1) {
                r.assert_held();
                caught = true;
            }
        }
    }
    CHECK_MSG(released, "Trigger never released the originally held board");
    CHECK_MSG(recall_intent, "Raw trigger never reached stock recall AG input");
    CHECK_MSG(recall_attribute, "RetrieveBoard animation never emitted physical recall pulse2476bit11");
    CHECK_MSG(returning, "Recall pulse never entered possession RETURNING4");
    CHECK_MSG(caught, "Real retrieval never completed into held board and armed hand drive");
}

// #[ignore = "requires private stock animation banks and collections"]
S3_TEST(raw_y_offboard_reaches_stock_remount_and_stops_hand_controller) {
    auto loaded = Replay::load();
    if (!loaded) return;
    auto& r = *loaded;
    bool mount_intent = false;
    bool mounting_attribute = false;
    bool mounted = false;
    for (std::size_t tick = 0; tick < 1800; ++tick) {
        r.tick(tick, tick == 20 || tick == 400 ? std::uint16_t(0x8000) : std::uint16_t(0), {0, 0});
        if (tick == 300) {
            CHECK_EQ(r.skater().player_state.current(), PhysicalStateId::BipedGround);
            r.assert_held();
        }
        const auto& s = r.skater();
        if (tick == 400) {
            CHECK_MSG(s.animation.motion.action_intents.contains_key("NewToggleOffBoardState"),
                      "Y while offboard was suppressed instead of reaching stock remount");
            CHECK(s.animation.motion.action_intents.contains_key("ToggleOffBoardState"));
            CHECK_MSG(!s.animation.motion.action_intents.contains_key("OB_RetrieveBoard"),
                      "Y was incorrectly replaced with trigger recall");
        }
        if (tick >= 400) {
            mount_intent = mount_intent || s.animation.motion.animation.motion_intents.contains_key("OB_Mount");
            mounting_attribute = mounting_attribute || (s.player_input.processed.flags_2480 & 0x80000u) != 0;
            const auto c = category(s.player_state.current());
            if (c == 100 || c == 200) {
                mounted = true;
                CHECK(!s.skateboard_controller.fields.system_on_452);
                CHECK_EQ(s.skateboard_controller.fields.state_448, 0u);
                CHECK_EQ(s.board_possession.state.selected_hand_424, 2u);
                for (const auto& hand : s.board_possession.state.hands) {
                    CHECK_MSG(hand_dynamics_released(hand.dynamics),
                              "Onboard physical state retains offboard hand drive at tick" << tick);
                }
            }
        }
    }
    CHECK_MSG(mount_intent, "Stock OffBoard AG never mapped Y to OB_Mount");
    CHECK_MSG(mounting_attribute, "Stock mount animation never emitted OB_Mounting");
    CHECK_MSG(mounted, "Stock remount never returned to an onboard physical state");
}
