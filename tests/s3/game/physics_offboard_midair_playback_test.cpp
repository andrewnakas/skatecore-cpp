// Port of skate-game/src/tests/offboard_midair_playback.rs (mounted in physics.rs as
// offboard_midair_playback).
//! Raw-input reproduction of dismounting while falling; no forced physical state.
// `#[ignore = ...]` private-asset tests: run on the cleanroom pack, skip without it.
// Diagnostic eprintln!s print a reduced field set (the Rust Debug dumps of whole structs have no
// C++ formatter); assertions are complete.
#include <cmath>
#include <cstdio>
#include <optional>

#include "physics_offboard_test_helpers.hpp"

using namespace s3test::game::offboard_playback;
namespace ground = s3::game::physics::ground;
using s3::player::state::category;

namespace {

float magnitude(const std::array<float, 4>& v) { return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
float distance(const std::array<float, 4>& a, const std::array<float, 4>& b) {
    std::array<float, 4> d{};
    for (int i = 0; i < 4; ++i) d[i] = a[i] - b[i];
    return magnitude(d);
}

void trace_air(const SkaterRuntime& skater, int tick) {
    const auto& p = skater.player_input.processed;
    const auto& air = skater.biped_air.state;
    const auto& selector = skater.offboard_air_selector.core;
    std::fprintf(stderr,
                 "Air trace tick%d: states=%u/%u categories=%u/%u timer=%g "
                 "flags2472/76/80/84/88=%08x/%08x/%08x/%08x/%08x; remaining=%g duration=%g "
                 "pending8492=%d requery8499=%d selection=%s\n",
                 tick, unsigned(p.state_2504), unsigned(p.state_2508), unsigned(p.category_2516),
                 unsigned(p.category_2512), double(p.state_timer_2664), unsigned(p.flags_2472),
                 unsigned(p.flags_2476), unsigned(p.flags_2480), unsigned(p.flags_2484), unsigned(p.flags_2488),
                 double(air.time_remaining_444), double(air.duration_448), int(selector.sampling.pending_8492),
                 int(selector.requery_pending_8499),
                 selector.selected_index ? std::to_string(*selector.selected_index).c_str() : "None");
}

void assert_physical_joints(const SkaterRuntime& skater, int tick) {
    if (const auto failure = joint_gap_failure(skater)) {
        // Same visible-failure acceptance bound as the real ground replay.
        CHECK_MSG(false, "Midair joint failure tick" << tick << ": " << failure->parent << " -> " << failure->child
                                                     << " gap=" << failure->gap
                                                     << "; board=" << skater.skateboard_controller.fields.state_448
                                                     << "; hand="
                                                     << skater.board_possession.state.selected_hand_424);
    }
}

void falling_dismount(bool release_board, float drop, bool complete_landing) {
    std::fprintf(stderr, "Midair scenario release_board=%d drop=%g complete_landing=%d\n", int(release_board),
                 double(drop), int(complete_landing));
    // A real authored drop gives the stock dismount animation time to run.
    // These are fixture dimensions, not gameplay constants or fake contacts.
    const float floor_y = ground::HEIGHT - drop;
    auto loaded = Playback::load_with("normal", false, [&](GamePhysics& physics) {
        physics.world = edge_world(physics.settings.floor_material, 2.0f, floor_y);
    });
    if (!loaded) return;
    auto& r = *loaded;
    auto& physics = r.physics;
    auto& skater = r.skater;
    std::optional<int> y_tick;
    std::optional<int> air_tick;
    int air_frames = 0;
    int continuity_frames = 0;
    bool saw_selector_completion = false;
    bool saw_held_board = false;
    std::optional<int> release_tick;
    int released_air_frames = 0;
    int landed_frames = 0;
    auto previous_state = skater.player_state.current();
    for (int tick = 0; tick < 1200; ++tick) {
        const auto before_velocity = skater.skeleton.record.centre_of_mass_velocity;
        const auto before_position = skater.skeleton.record.centre_of_mass;
        const auto before_state = skater.player_state.current();
        const auto before_board_state = skater.skateboard_controller.fields.state_448;
        const bool falling = category(before_state) == 200 && before_velocity[1] < 0.0f;
        const bool press_y = !y_tick && falling;
        std::uint16_t buttons = 0;
        if (press_y) {
            y_tick = tick;
            buttons = 0x8000;
        } else if (!y_tick && tick >= 12 && category(before_state) != 200) {
            buttons = 0x1000;
        }
        //Use the same raw RT input as the verified held->released ground
        //replay, after actual Air entry. The stock graph owns its timing.
        auto actions = r.sample_for_physics(xbox(buttons,
                                                 release_board && air_frames > 0 ? std::array<std::uint8_t, 2>{0, 255}
                                                                                 : std::array<std::uint8_t, 2>{0, 0},
                                                 {0, 0}, {0, 0}));
        const auto advanced = r.advance(actions);
        CHECK_MSG(advanced.is_ok(), "Midair dismount tick" << tick << ", Y="
                                                          << (y_tick ? std::to_string(*y_tick) : "None") << ", prior="
                                                          << static_cast<unsigned>(before_state) << ": "
                                                          << advanced.unwrap_err());
        const auto state = skater.player_state.current();
        if (state != previous_state || press_y || before_board_state != skater.skateboard_controller.fields.state_448) {
            trace_air(skater, tick);
            const auto& com = skater.skeleton.record.centre_of_mass;
            const auto& v = skater.skeleton.record.centre_of_mass_velocity;
            std::fprintf(stderr,
                         "Midair tick%d: %u->%u, Y=%d; COM=[%g, %g, %g]; velocity=[%g, %g, %g]; board=%u; hand=%u\n",
                         tick, unsigned(previous_state), unsigned(state), y_tick ? *y_tick : -1, double(com[0]),
                         double(com[1]), double(com[2]), double(v[0]), double(v[1]), double(v[2]),
                         unsigned(skater.skateboard_controller.fields.state_448),
                         unsigned(skater.board_possession.state.selected_hand_424));
            previous_state = state;
        }
        if (state == PhysicalStateId::BipedAir) {
            CHECK_MSG(y_tick.has_value(), "BipedAir preceded requested dismount at tick" << tick);
            if (!air_tick) air_tick = tick;
            air_frames += 1;
            const auto board_state = skater.skateboard_controller.fields.state_448;
            saw_held_board = saw_held_board || board_state == 1;
            if (release_board && board_state == 2) {
                CHECK_MSG(saw_held_board, "Released scenario never held the board in Air");
                if (!release_tick) release_tick = tick;
                released_air_frames += 1;
            }
            const auto& selector = skater.offboard_air_selector.core;
            saw_selector_completion =
                saw_selector_completion || (!selector.sampling.pending_8492 && selector.selected_index.has_value());
            CHECK_MSG(skater.skateboard_controller.fields.system_on_452,
                      "Air entry left the canonical possession controller off at tick" << tick);
        }
        if (y_tick) {
            //Air548 remains set until its native landing condition. Ground
            //selection while it is set exposes the wrong-width flag read.
            if (before_state == PhysicalStateId::BipedAir && state == PhysicalStateId::BipedGround &&
                skater.biped_air.state.flags_544_550[4]) {
                CHECK_MSG(false, "Air selected Ground with retained Air328=true at tick" << tick);
            }
            assert_physical_joints(skater, tick);
            const auto velocity = skater.skeleton.record.centre_of_mass_velocity;
            bool finite = true;
            for (float v : velocity) finite = finite && std::isfinite(v);
            CHECK_MSG(finite, "Nonfinite actual COM velocity at tick" << tick);
            bool root_finite = true;
            for (float v : skater.skeleton_input.root_velocity) root_finite = root_finite && std::isfinite(v);
            CHECK_MSG(root_finite, "Nonfinite drive-root velocity at tick" << tick);
            CHECK_MSG(render_pose_finite(skater), "Nonfinite rendered pose at tick" << tick);
            CHECK_MSG(camera_position_finite(r.camera), "Nonfinite gameplay camera at tick" << tick);
            // Acceptance bounds, not original force/solver constants. Apply only
            // during the first four Air/release ticks, away from the floor;
            // contact impulses on later landing are not a continuity failure.
            if (state == PhysicalStateId::BipedAir && (air_frames <= 4 || (release_tick && tick < *release_tick + 4)) &&
                before_position[1] > floor_y + 2.0f) {
                continuity_frames += 1;
                const float delta_velocity = distance(velocity, before_velocity);
                const float displacement = distance(skater.skeleton.record.centre_of_mass, before_position);
                const float travel = magnitude(before_velocity) * physics.settings.step.simulation.time_step;
                CHECK_MSG(delta_velocity < 10.0f,
                          "Midair COM velocity discontinuity tick" << tick << ": delta=" << delta_velocity);
                CHECK_MSG(displacement < travel + 0.5f, "Midair COM position discontinuity tick"
                                                            << tick << ": step=" << displacement
                                                            << ", previous-speed travel=" << travel);
            }
            if (skater.skateboard_controller.fields.state_448 == 2) {
                const std::array<std::uint32_t, 8> released{0, 0, 0, 2, 0, 0, 0, 2};
                CHECK_MSG(physics.board.hook().drive.dynamics == released, "Released deck still driven tick" << tick);
                CHECK_MSG(skater.board_possession.state.selected_hand_424 == 2,
                          "Released board still has a hand owner tick" << tick);
                for (const auto& hand : skater.board_possession.state.hands) {
                    CHECK_MSG(hand_dynamics_released(hand.dynamics), "Released hand drive still armed tick" << tick);
                }
            }
        }
        // Continue beyond entry and query completion, but do not turn this
        // isolated reproduction into a wipeout/recovery feature acceptance.
        if (air_tick && state == PhysicalStateId::BipedGround) landed_frames += 1;
        const bool done = complete_landing ? landed_frames >= 120
                                           : ((!release_board && air_frames >= 12) ||
                                              (release_board && released_air_frames >= 24));
        if (done) break;
    }
    if (complete_landing) {
        CHECK_MSG(landed_frames >= 120, "Dismount drop=" << drop << ", released=" << release_board
                                                         << " did not sustain landing: " << landed_frames << "; state="
                                                         << static_cast<unsigned>(skater.player_state.current()));
    }
    CHECK_MSG(y_tick.has_value(), "Raw push never produced observed falling onboard Air");
    CHECK_MSG(air_tick.has_value(), "Falling raw Y never entered BipedAir");
    CHECK_MSG(air_frames >= 12, "BipedAir did not survive twelve update frames");
    CHECK_MSG(continuity_frames >= 1, "No airborne velocity-continuity interval observed");
    CHECK_MSG(saw_selector_completion, "No real selector candidate completed while BipedAir was observed");
    if (release_board) {
        CHECK_MSG(release_tick.has_value(), "Raw RT never released the held board in Air");
        //The original20m continuity fixture must still cover24 release ticks.
        //A2m drop lands sooner; the new case instead requires120 landed ticks
        //above, plus proof the raw release actually occurred while airborne.
        const int required_release_frames = complete_landing ? 1 : 24;
        CHECK_MSG(released_air_frames >= required_release_frames, "Did not observe" << required_release_frames
                                                                                    << " released-board Air ticks: "
                                                                                    << released_air_frames);
    }
}

}  // namespace

// #[ignore = "requires private stock animation banks and collections"]
S3_TEST(raw_y_while_falling_preserves_biped_air_joint_and_velocity_continuity) {
    for (const bool release_board : {false, true}) falling_dismount(release_board, release_board ? 20.0f : 8.0f, false);
}

// #[ignore = "requires private stock assets; complete midair dismount/landing regression"]
S3_TEST(raw_midair_dismount_at_two_heights_runs_through_sustained_landing) {
    for (const float drop : {2.0f, 6.0f}) {
        for (const bool released : {false, true}) falling_dismount(released, drop, true);
    }
}
