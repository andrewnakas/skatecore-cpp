// Port of skate-game/src/tests/air_playback.rs (mounted in physics.rs as `air_tests`).
//! Real controller/graph/physical/pose/camera regression across a level edge.
// The Rust tests are `#[ignore = "requires private stock animation banks and collections"]`; here
// they run against the cleanroom pack and skip without it.
#include <cmath>
#include <cstdio>
#include <optional>

#include "physics_playback_support.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/collision.hpp"
#include "s3/core/player/state.hpp"

using namespace s3::game;
using namespace s3::game::physics;
using s3::player::state::PhysicalStateId;
using s3::physics::board_world::BoardWorld;
using s3::physics::board_world::WorldTriangle;
namespace pb = s3test::game::playback;

namespace {

std::string opt_f32(const float* v) { return v ? "Some(" + fmt::debug_f32(*v) + ")" : "None"; }
std::string vec3(const s3::math::Vector3& v) {
    return "(" + fmt::debug_f32(v.x) + ", " + fmt::debug_f32(v.y) + ", " + fmt::debug_f32(v.z) + ")";
}

void roll_off_sequence(std::optional<std::int16_t> air_stick, std::optional<std::size_t> grab_hand,
                       std::array<std::int16_t, 2> grab_stick, bool tailwalk) {
    const auto* s = pb::stock();
    if (s == nullptr) return;
    auto physics = pb::unwrap(GamePhysics::load_with_terrain(*s->vfs, s->root, ground::Terrain::Course), "physics");
    // Keep this air fixture's open edge now that the production level has a
    // downhill ramp. The dedicated ramp regression uses the unmodified world.
    {
        std::vector<WorldTriangle> kept;
        for (const auto& face : physics.world.triangles()) {
            bool beyond = false;
            for (const auto& vertex : face.triangle.vertices) beyond = beyond || vertex.z > 4.0f;
            if (!(face.tag == 1 && beyond)) kept.push_back(face);
        }
        physics.world = BoardWorld::new_(std::move(kept));
    }
    if (grab_hand) {
        // A deeper test landing gives the held cycle and OUT animation time
        // to run before contact can hide a failed airborne release.
        std::vector<WorldTriangle> deeper;
        for (const auto& face : physics.world.triangles()) {
            auto vertices = face.triangle.vertices;
            for (auto& vertex : vertices) {
                if (vertex.y < -1.0f) vertex.y -= 18.0f;
            }
            auto rebuilt = WorldTriangle::from_vertices(vertices, face.material, face.tag,
                                                        s3::physics::collision::TriangleFeature::ONE_SIDED,
                                                        {1.0f, 1.0f, 1.0f}, 0.0f);
            deeper.push_back(s3::rs::unwrap(rebuilt));
        }
        physics.world = BoardWorld::new_(std::move(deeper));
    }
    auto skater = pb::unwrap(SkaterRuntime::load(*s->vfs, s->root, s->graphs, physics, "normal"), "skater");
    PlayerControls controls{};
    input::controllers::ControllerInput input{};
    auto camera = pb::unwrap(camera::CameraRuntime::load(*s->vfs, s->root), "camera");
    auto previous = skater.player_state.current();
    bool saw_air = false;
    int landed_frames = 0;
    int airborne_frames = 0;
    int grab_frames = 0;
    bool saw_grab = false;
    bool saw_release = false;
    for (int tick = 0; tick < 1000; ++tick) {
        std::array<std::uint8_t, 2> triggers{0, 0};
        if (grab_hand) {
            if (saw_air && grab_frames < (tailwalk ? 100 : 35)) triggers[*grab_hand] = 255;
        }
        const std::uint16_t buttons = (tick >= 12 && !saw_air)                     ? 0x1000
                                      : (tailwalk && saw_air && grab_frames >= 20) ? 0x2000
                                                                                   : 0;
        const std::array<std::int16_t, 2> left{saw_air ? air_stick.value_or(0) : std::int16_t(0), 0};
        const std::array<std::int16_t, 2> right =
            (triggers != std::array<std::uint8_t, 2>{0, 0}) ? grab_stick : std::array<std::int16_t, 2>{0, 0};
        const auto result =
            pb::tick(physics, skater, controls, input, camera, *s, pb::xbox(buttons, triggers, left, right), false);
        const auto state = skater.player_state.current();
        if (state != previous) {
            std::fprintf(stderr, "Roll-off tick%d: %u -> %u; deck=%s; velocity=%s\n", tick, unsigned(previous),
                         unsigned(state), vec3(physics.board.bodies()[6].rates.position).c_str(),
                         vec3(physics.board.bodies()[6].rates.linear_velocity).c_str());
            previous = state;
        }
        if (!result.is_ok()) {
            pb::panic("Roll-off tick" + std::to_string(tick) + ": " + result.unwrap_err() + "; selected=" +
                      std::to_string(unsigned(skater.player_state.requested_state)) + "; flags=" +
                      pb::hex8(skater.player_input.processed.flags_2468) + "/" +
                      pb::hex8(skater.player_input.processed.flags_2472) + "/" +
                      pb::hex8(skater.player_input.processed.flags_2476));
        }
        saw_air = saw_air || s3::player::state::category(state) == 200;
        if (tailwalk && s3::player::state::category(state) == 200) {
            const auto& motion = skater.animation.motion;
            if (grab_frames >= 20) {
                std::fprintf(stderr,
                             "Tailwalk frame=%d LeftPush=%s RightPush=%s angle=%s TailGrab=%s NoFootAirWalk=%s "
                             "animation=%s\n",
                             grab_frames, opt_f32(controls.action_intents.get("LeftPush")).c_str(),
                             opt_f32(controls.action_intents.get("RightPush")).c_str(),
                             opt_f32(controls.action_intents.get("BoardAdjustAngle")).c_str(),
                             opt_f32(motion.animation.motion_intents.get("TailGrab")).c_str(),
                             opt_f32(motion.animation.motion_intents.get("NoFootAirWalk")).c_str(),
                             motion.animation.current_name.value_or("None").c_str());
            }
            if (motion.animation.current_name == std::optional<std::string>("2FT_AIR_GRAB_N_TAIL_0_CYC")) {
                CHECK(motion.animation.motion_intents.contains_key("TailGrab"));
                CHECK(motion.animation.motion_intents.contains_key("NoFootAirWalk"));
                CHECK(motion.score_packet.grab.has_value());
                CHECK(motion.score_packet.grab->first ==
                      s3::animation::skeleton_input::name::encode("tailgrab_airwalk"));
                CHECK_EQ(skater.pose_generation, physics.ticks);
                return;
            }
        }
        if (grab_hand && s3::player::state::category(state) == 200) {
            grab_frames += 1;
            const auto& anim = skater.animation.motion.animation;
            const bool grabbing = (skater.player_input.processed.flags_2468 & 0x20u) != 0;
            saw_grab = saw_grab || grabbing;
            saw_release = saw_release || (saw_grab && grab_frames > 35 && !grabbing);
            if (grab_frames == 3 || grab_frames == 34 || grab_frames == 36 || grab_frames == 50 || grab_frames == 65) {
                std::fprintf(stderr,
                             "Grab hand=%zu stick=[%d, %d] frame=%d trigger=[%u, %u] AG=%s/%s MG=%s/%s animation=%s "
                             "flags=%s\n",
                             *grab_hand, grab_stick[0], grab_stick[1], grab_frames, triggers[0], triggers[1],
                             opt_f32(controls.action_intents.get("LeftAirGrab")).c_str(),
                             opt_f32(controls.action_intents.get("RightAirGrab")).c_str(),
                             opt_f32(anim.motion_intents.get("WantsLeftAirGrab")).c_str(),
                             opt_f32(anim.motion_intents.get("WantsRightAirGrab")).c_str(),
                             anim.current_name.value_or("None").c_str(),
                             pb::hex8(skater.player_input.processed.flags_2468).c_str());
            }
        }
        if (saw_air && air_stick) {
            airborne_frames += 1;
            std::fprintf(stderr, "Air stick=%d frame%d: AG=%s/%s MG=%s/%s speed=%s angle=%s\n", *air_stick,
                         airborne_frames, opt_f32(controls.action_intents.get("BodySpin")).c_str(),
                         opt_f32(controls.action_intents.get("PhysBodySpin")).c_str(),
                         opt_f32(skater.animation.motion.animation.motion_intents.get("BodySpin")).c_str(),
                         opt_f32(skater.animation.motion.animation.motion_intents.get("PhysBodySpin")).c_str(),
                         fmt::display_f32(skater.air_reckoning.state.spin_speed).c_str(),
                         fmt::display_f32(skater.air_reckoning.state.spin_angle).c_str());
            if (airborne_frames == 16) {
                CHECK_MSG(std::fabs(skater.air_reckoning.state.spin_angle) > 0.001f,
                          "Raw airborne stick did not reach physical angular integration");
                return;
            }
        }
        if (saw_air && state == PhysicalStateId::PhysicsGround && physics.riding.ground.wheel_contact_count > 0) {
            landed_frames += 1;
        }
        CHECK(camera.frame.has_value());
        CHECK_EQ(skater.pose_generation, physics.ticks);
        if (landed_frames >= 120) break;
    }
    CHECK_MSG(saw_air, "Raw pushing never entered the actual Air family");
    CHECK_MSG(!tailwalk, "Raw airborne LT + right stick up + B did not produce the Tailwalk cycle");
    if (grab_hand) {
        CHECK_MSG(saw_grab, "Grab never became active");
        CHECK_MSG(saw_release, "Grab never released while airborne");
    }
    CHECK_MSG(landed_frames >= 120, "Air never returned to stable grounded riding");
    std::fprintf(stderr, "Completed roll-off/landing: ticks=%llu; grounded_after_air=%d\n",
                 static_cast<unsigned long long>(physics.ticks), landed_frames);
}

void roll_off_input(std::optional<std::int16_t> air_stick, std::optional<std::size_t> grab_hand,
                    std::array<std::int16_t, 2> grab_stick) {
    roll_off_sequence(air_stick, grab_hand, grab_stick, false);
}

void roll_off(std::optional<std::int16_t> air_stick) { roll_off_input(air_stick, std::nullopt, {0, 0}); }

}  // namespace

// #[ignore = "requires private stock animation banks and collections"]
S3_TEST(stock_push_off_edge_runs_air_and_landing_in_production) {
    if (s3test::game::skip_stale("the test swaps physics.world for BoardWorld::new(triangles) without query metadata; skate-core returns 'Canonical world has no authored query metadata' (board_world.rs query_metadata)")) return;
    roll_off(std::nullopt);
}

// #[ignore = "requires private stock animation banks and collections"]
S3_TEST(airborne_stick_reaches_physical_spin) {
    for (const std::int16_t stick : {std::int16_t(-32767), std::int16_t(32767)}) roll_off(stick);
}

// #[ignore = "requires private stock animation banks and collections"]
S3_TEST(airborne_grabs_release_through_stock_graphs) {
    if (s3test::game::skip_stale("the test swaps physics.world for BoardWorld::new(triangles) without query metadata; skate-core returns 'Canonical world has no authored query metadata' (board_world.rs query_metadata)")) return;
    for (std::size_t hand = 0; hand < 2; ++hand) {
        for (const auto stick : {std::array<std::int16_t, 2>{0, 0}, std::array<std::int16_t, 2>{32767, 0},
                                 std::array<std::int16_t, 2>{-32767, 0}, std::array<std::int16_t, 2>{0, 32767},
                                 std::array<std::int16_t, 2>{0, -32767}}) {
            roll_off_input(std::nullopt, hand, stick);
        }
    }
}

// #[ignore = "requires private stock animation banks and collections"]
S3_TEST(airborne_tailwalk_from_raw_controller_reaches_stock_cycle) {
    if (s3test::game::skip_stale("the test swaps physics.world for BoardWorld::new(triangles) without query metadata; skate-core returns 'Canonical world has no authored query metadata' (board_world.rs query_metadata)")) return;
    roll_off_sequence(std::nullopt, std::size_t(0), {0, 32767}, true);
}

// #[ignore = "requires private stock animation banks and collections"]
S3_TEST(stock_starting_ramp_reaches_floor_without_air) {
    const auto* s = pb::stock();
    if (s == nullptr) return;
    auto physics = pb::unwrap(GamePhysics::load_with_terrain(*s->vfs, s->root, ground::Terrain::Course), "physics");
    auto skater = pb::unwrap(SkaterRuntime::load(*s->vfs, s->root, s->graphs, physics, "normal"), "skater");
    PlayerControls controls{};
    input::controllers::ControllerInput input{};
    auto camera = pb::unwrap(camera::CameraRuntime::load(*s->vfs, s->root), "camera");
    for (int tick = 0; tick < 600; ++tick) {
        const auto result = pb::tick(physics, skater, controls, input, camera, *s,
                                     pb::xbox(tick >= 12 ? 0x1000 : 0, {0, 0}, {0, 0}, {0, 0}), false);
        if (!result.is_ok()) pb::panic("Ramp tick" + std::to_string(tick) + ": " + result.unwrap_err());
        CHECK_MSG(s3::player::state::category(skater.player_state.current()) != 200,
                  "Ramp entered air at tick" << tick << ", deck=" << vec3(physics.board.bodies()[6].rates.position));
        const auto deck = physics.board.bodies()[6].rates.position;
        if (deck.z > 19.0f) {
            CHECK_MSG(deck.y < ground::FLOOR_HEIGHT + 0.5f, "Did not descend to floor: " << vec3(deck));
            CHECK(physics.riding.ground.wheel_contact_count > 0);
            std::fprintf(stderr, "Reached lower floor without air at tick%d: %s\n", tick, vec3(deck).c_str());
            return;
        }
    }
    pb::panic("Did not reach the lower floor within 600 ticks");
}
