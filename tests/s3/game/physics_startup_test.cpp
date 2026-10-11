// Port of skate-game/src/tests/physics_startup.rs (mounted as physics::tests). These Rust tests are
// `#[ignore]` private-asset tests; here they run against the cleanroom pack and skip without it.
// Two tests call `crate::customiser::apply_preferences` (skate-game; not part of the Bevy-free
// simref): `apply_preferences` below ports its body for the JSON keys the tests pass.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "game_test_assets.hpp"
#include "harness.hpp"
#include "s3/core/animation/playback.hpp"
#include "s3/core/animation/playback_tree.hpp"
#include "s3/core/camera/slow_motion.hpp"
#include "s3/core/input/xbox.hpp"
#include "s3/core/physics/board.hpp"
#include "s3/core/physics/collision.hpp"
#include "s3/data/collections.hpp"
#include "s3/data/manifest.hpp"
#include "s3/game/camera.hpp"
#include "s3/game/graph_runtime.hpp"
#include "s3/game/input/controllers.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/colliders.hpp"
#include "s3/game/physics/controls.hpp"
#include "s3/game/physics/frame.hpp"
#include "s3/game/physics/ground_phase.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/skeleton_colliders.hpp"

using namespace s3::game;
using namespace s3::game::physics;
using s3::math::Vector3;
using s3::physics::board::BodyId;
using s3::player::state::PhysicalStateId;

namespace {

std::string err_text(const s3::game::Status& s) { return s.is_ok() ? std::string() : s.unwrap_err(); }

s3::input::xbox::XboxState xbox(std::uint16_t buttons, std::array<std::uint8_t, 2> triggers,
                                std::array<std::int16_t, 2> left) {
    s3::input::xbox::XboxState state{};
    state.buttons = buttons;
    state.triggers = triggers;
    state.left = left;
    state.right = {0, 0};
    return state;
}

/// Profile keys of skate-game `customiser::apply_preferences` (`scalar(profile, key, initial)`);
/// absent keys take the same defaults. No test passes "gestures", so `set_gesture_preferences(None)`.
struct Preferences {
    double truck = 0.7;
    double wheel = 0.7;
    double stance = 1.0;
    double style = 0.0;
    double posture = 0.0;
};
void apply_preferences(const Preferences& profile, GamePhysics& physics, skater_animation::SkaterAnimation& animation) {
    physics.set_equipment_preferences(static_cast<float>(profile.truck), static_cast<float>(profile.wheel));
    physics.set_gesture_preferences(std::nullopt);
    animation.set_customisation(static_cast<std::uint32_t>(profile.stance), static_cast<std::uint32_t>(profile.style));
    animation.motion.animation.posture.set_profile(static_cast<std::uint32_t>(profile.posture));
}

const s3::data::Vfs* pack() { return s3test::game::assets(); }

}  // namespace

// #[ignore = "requires the user's converted stock skater and graph assets"]
S3_TEST(stock_skater_startup_builds_the_graphs_and_physical_body_from_the_same_rig) {
    if (s3test::game::skip_stale("the test swaps physics.world for BoardWorld::new(triangles) without query metadata; skate-core returns 'Canonical world has no authored query metadata' (board_world.rs query_metadata)")) return;
    const auto* vfs = pack();
    if (!vfs) return;
    const std::string& root = s3test::game::ASSET_ROOT;
    auto manifest = s3::data::GameAssets::load(*vfs, root);
    CHECK_MSG(manifest.ok, manifest.error);
    auto graphs = graph_runtime::StockGraphs::load(*vfs, root, manifest.value).unwrap();
    auto physics = GamePhysics::load(*vfs, root).unwrap();
    // This regression isolates grounded controls on the original flat fixture.
    // The production box is only six metres wide: sustained kickturning leaves
    // it at x=-3.32, correctly selecting KnownAir and ForcePhysics2. Separate
    // air_playback tests exercise the shipped course and deliberate roll-off.
    const std::array<Vector3, 4> corners{
        Vector3{-20.0f, ground::HEIGHT, -20.0f},
        Vector3{20.0f, ground::HEIGHT, -20.0f},
        Vector3{20.0f, ground::HEIGHT, 20.0f},
        Vector3{-20.0f, ground::HEIGHT, 20.0f},
    };
    {
        std::vector<s3::physics::board_world::WorldTriangle> triangles;
        for (const auto& indices : {std::array<int, 3>{0, 2, 1}, std::array<int, 3>{0, 3, 2}}) {
            triangles.push_back(s3::rs::unwrap(s3::physics::board_world::WorldTriangle::from_vertices(
                {corners[indices[0]], corners[indices[1]], corners[indices[2]]}, physics.settings.floor_material, 0,
                s3::physics::collision::TriangleFeature::ONE_SIDED, {1.0f, 1.0f, 1.0f}, 0.0f)));
        }
        physics.world = s3::physics::board_world::BoardWorld::new_(std::move(triangles));
    }
    auto skater = SkaterRuntime::load(*vfs, root, graphs, physics, "normal").unwrap();
    CHECK_EQ(skater.animation.pose.size(), skater.animation.evaluator->frames.bone_names.size());
    CHECK_EQ(skater.skeleton.bodies().size(), std::size_t(26));
    //InitJoints uses the raw animation frames, while physical part setup
    //orthonormalizes its frames. Coincident startup anchors are not a source
    //guarantee; validate finite geometry and the real compiled joint rows.
    {
        const auto parts = skater.skeleton.part_transforms();
        auto point = [&](std::size_t part, const std::uint32_t* anchor) {
            std::array<float, 3> p{};
            for (int i = 0; i < 3; ++i) p[i] = s3::rs::from_bits(anchor[i]);
            std::array<float, 3> out{};
            for (int i = 0; i < 3; ++i) {
                const float x = std::fma(parts[part][0][i], p[0], parts[part][3][i]);
                const float y = std::fma(parts[part][1][i], p[1], x);
                out[i] = std::fma(parts[part][2][i], p[2], y);
            }
            return out;
        };
        for (const auto& joint : skater.skeleton_joints.records) {
            const auto child = point(joint.child, &joint.frames.words[4]);
            const auto parent = point(joint.parent, &joint.frames.words[12]);
            bool finite = true;
            for (int i = 0; i < 3; ++i) finite = finite && std::isfinite(child[i]) && std::isfinite(parent[i]);
            CHECK(finite);
        }
    }
    const auto joints =
        skater.skeleton_joints.build(skater.skeleton.bodies(), 8, physics.settings.step.simulation.time_step);
    CHECK_EQ(joints.size(), std::size_t(22));
    const auto drives =
        skater.skeleton_drives.build(skater.skeleton.bodies(), 8, 34, physics.settings.step.simulation.time_step);
    CHECK_EQ(drives.rows.size(), std::size_t(48));
    for (const auto& drive : drives.rows) {
        CHECK(drive.frame_a_body.reaction_index >= 8 && drive.frame_a_body.reaction_index < 38 &&
              drive.frame_b_body.reaction_index >= 8 && drive.frame_b_body.reaction_index < 38);
    }
    CHECK(skater.skeleton_drives.targets.transform(2)[3] ==
          skater.animated_skeleton.board_frames.lifted_com_frame[3]);
    CHECK(skater.skeleton_drives.targets.transform(3)[3] == skater.animated_skeleton.board_frames.com_frame[3]);
    for (const auto& j : joints) {
        CHECK(j.reaction_a >= 8 && j.reaction_a < 34 && j.reaction_b >= 8 && j.reaction_b < 34);
    }
    const auto com = skater.skeleton.record.centre_of_mass;
    for (const float v : com) CHECK(std::isfinite(v));
    const auto deck = physics.board.part_transforms()[s3::physics::board::index(BodyId::Deck)];
    CHECK(com[1] > deck.translation.y);
    {
        const auto parts = skater.skeleton.part_transforms();
        for (std::size_t i = 0; i < skater.skeleton.bodies().size(); ++i) {
            const auto& body = skater.skeleton.bodies()[i];
            CHECK(std::isfinite(body.inertia.inverse_mass) && body.inertia.inverse_mass > 0.0f);
            for (const auto& row : parts[i]) {
                for (const float v : row) CHECK(std::isfinite(v));
            }
        }
    }
    for (const auto& v : skater.skeleton.record.velocities) {
        for (const float x : v) CHECK(x == 0.0f);
    }
    const auto volumes = skeleton_colliders::world_volumes(skater.skeleton, skater.skeleton_collision).unwrap();
    CHECK_EQ(volumes.size(), std::size_t(23));
    auto positions = [&]() {
        std::array<Vector3, 26> out{};
        for (std::size_t i = 0; i < out.size(); ++i) out[i] = skater.skeleton.bodies()[i].rates.position;
        return out;
    };
    const auto before = positions();
    const auto render_before = skater.render_pose;
    PlayerControls controls{};
    auto camera = camera::CameraRuntime::load(*vfs, root).unwrap();
    input::controllers::ControllerInput input{};
    const float physical_step = physics.settings.step.simulation.time_step;
    const auto normal_period = physics.clock.period();
    camera.simulation_rate_requests.push_back(s3::camera::SimulationRateRequest{1.0f / 35.0f, 1});
    {
        auto actions = input.player_actions();
        const auto advanced = frame::advance(physics, skater, controls, graphs, actions, false, camera);
        CHECK_MSG(advanced.is_ok(), err_text(advanced));
    }
    CHECK_EQ(physics.ticks, std::uint64_t(1));
    CHECK(camera.simulation_rate_requests.empty());
    CHECK(physics.clock.period().as_nanos() > normal_period.as_nanos());
    CHECK_EQ(physics.settings.step.simulation.time_step, physical_step);
    CHECK_EQ(skater.player_input.processed.timestep_2604, physical_step);
    //Ctor does not publish Ground before the initial input reset. The stock
    //graph enters Ground on the next tick and its Begin writes ForcePhysics1.
    CHECK_EQ(skater.skeleton_input.force_mode, 0u);
    CHECK_MSG(camera.frame.has_value(), "First frame did not publish the gameplay camera");
    {
        const auto& deck_position = physics.board.bodies()[6].rates.position;
        const auto& first_com = skater.skeleton.record.centre_of_mass;
        std::fprintf(stderr, "First gameplay frame: deck=(%g, %g, %g); COM=(%g, %g, %g)\n",
                     static_cast<double>(deck_position.x), static_cast<double>(deck_position.y),
                     static_cast<double>(deck_position.z), static_cast<double>(first_com[0]),
                     static_cast<double>(first_com[1]), static_cast<double>(first_com[2]));
    }
    CHECK_EQ(skater.pose_generation, physics.ticks);
    CHECK(render_before != skater.render_pose);
    for (const auto& m : skater.render_pose) {
        for (const auto& row : m) {
            for (const float v : row) CHECK(std::isfinite(v));
        }
    }
    CHECK_EQ(s3::rs::unwrap(skater.solved_drives).rows.size(), std::size_t(48));
    CHECK(before != positions());
    // User crash: standing kickturn after spawn, left stick nearly full left,
    // no right-stick/trigger/button input. Stock Riding retains ForcePhysics1.
    bool saw_kickturn_balance = false;
    for (int tick = 0; tick < 1800; ++tick) {
        input.sample_raw_for_test(xbox(0, {0, 0},
                                       (tick >= 12 && tick < 612) ? std::array<std::int16_t, 2>{-32768, 256}
                                                                  : std::array<std::int16_t, 2>{0, 0}));
        auto actions = input.player_actions();
        controls.update(actions, physical_step, physics.settings.input_magnitude_threshold,
                        skater.player_input.physical.scoring.capabilities_204);
        const auto advanced = frame::advance(physics, skater, controls, graphs, actions, true, camera);
        CHECK_MSG(advanced.is_ok(), "Standing kickturn tick" << tick << ": " << err_text(advanced)
                                                             << "; force_mode=" << skater.skeleton_input.force_mode
                                                             << "; board_axis_y="
                                                             << skater.skeleton_input.drive_frames[0][2][1]);
        CHECK_MSG(skater.skeleton_input.force_mode == 1u,
                  "Stock Ground ForcePhysics Begin was lost at standing tick"
                      << tick << "; state=" << static_cast<std::uint32_t>(skater.player_state.current())
                      << "; contacts=" << physics.riding.ground.wheel_contact_count);
        saw_kickturn_balance = saw_kickturn_balance || skater.animation_input.fields.balance != 0.0f;
    }
    CHECK_MSG(saw_kickturn_balance, "Standing input never reached the stock kickturn balance");
    // Use raw signed axes and XInput button bits through the same converter,
    // history, Pad and action map as the game. This verifies connectivity;
    // matched original-game recordings and hardware polling remain separate.
    const auto start = physics.board.bodies()[6].rates.position;
    bool saw_push = false;
    bool saw_steering = false;
    for (int tick = 0; tick < 180; ++tick) {
        std::array<std::int16_t, 2> left{0, 0};
        if (tick < 20) {
            left = {static_cast<std::int16_t>(tick % 2 == 0 ? 8191 : -8191), 0};
        } else if (tick >= 90 && tick < 120) {
            left = {32767, 0};
        } else if (tick >= 120 && tick < 150) {
            left = {-32768, 0};
        } else if (tick >= 150 && tick < 180) {
            left = {32767, 32767};
        }
        input.sample_raw_for_test(xbox((tick >= 20 && tick < 130) ? 0x1000 : 0, {0, 0}, left));
        if (tick < 20) {
            CHECK_MSG(input.mapped_actions[0][0] == 0.0f && input.mapped_actions[0][1] == 0.0f,
                      "Sub-dead-zone stick drift reached gameplay actions");
        }
        auto action = input.player_actions();
        controls.update(action, physics.settings.step.simulation.time_step, physics.settings.input_magnitude_threshold,
                        skater.player_input.physical.scoring.capabilities_204);
        const auto advanced = frame::advance(physics, skater, controls, graphs, action, true, camera);
        CHECK_MSG(advanced.is_ok(), "Production tick" << tick << ": " << err_text(advanced) << "; state="
                                                      << static_cast<std::uint32_t>(skater.player_state.current())
                                                      << "; flags=" << std::hex
                                                      << skater.player_input.processed.flags_2468 << std::dec
                                                      << "; contacts=" << physics.riding.ground.wheel_contact_count
                                                      << "; speed=" << physics.riding.motion.ground_speed
                                                      << "; force_mode=" << skater.skeleton_input.force_mode);
        saw_push = saw_push || (skater.player_input.processed.flags_2468 & (1u << 25)) != 0;
        for (const float v : skater.ground.steering.targets) saw_steering = saw_steering || std::fabs(v) > 0.001f;
        if (tick == 119 || tick == 149 || tick == 179) {
            std::fprintf(stderr, "Raw stick=[%d, %d]; mapped=[%g, %g]; physical Turn=%g\n", left[0], left[1],
                         static_cast<double>(input.mapped_actions[0][0]),
                         static_cast<double>(input.mapped_actions[0][1]),
                         static_cast<double>(skater.animation_input.fields.turn));
            CHECK_MSG(std::fabs(skater.animation_input.fields.turn) > 0.001f,
                      "Raw stick input did not reach physical steering");
        }
    }
    const auto end = physics.board.bodies()[6].rates.position;
    CHECK(physics.clock.period() == normal_period);
    CHECK_MSG(saw_push, "Stock push animation never reached physical push input");
    CHECK_MSG(saw_steering, "Stock steering never reached truck drive targets");
    CHECK_MSG(std::hypot(end.x - start.x, end.z - start.z) > 0.01f, "Pushing did not move the live board");
    CHECK_MSG(physics.riding.ground.wheel_contact_count > 0,
              "Grounded riding lost every wheel contact on the flat test surface");
    CHECK_MSG(end.y > ground::HEIGHT, "The live deck penetrated the flat test surface");
    CHECK_MSG(camera.frame.has_value(), "Gameplay camera never published");
    CHECK_EQ(skater.pose_generation, physics.ticks);
    CHECK_EQ(skater.animation.ticks, physics.ticks);
    const float speed_before_brake = std::fabs(physics.riding.motion.ground_speed);
    bool saw_brake = false;
    for (int tick = 180; tick < 360; ++tick) {
        input.sample_raw_for_test(xbox(0x2000, {0, 0}, {0, 0}));
        auto action = input.player_actions();
        controls.update(action, physics.settings.step.simulation.time_step, physics.settings.input_magnitude_threshold,
                        skater.player_input.physical.scoring.capabilities_204);
        const auto advanced = frame::advance(physics, skater, controls, graphs, action, true, camera);
        CHECK_MSG(advanced.is_ok(), "Braking tick" << tick << ": " << err_text(advanced));
        // Brake82BDB0D4 writes the scalar consumed by Ground forces;
        // flags2468 bit29 is the separate ManualBrake attribute.
        saw_brake = saw_brake || skater.animation_input.fields.brake > 0.0f;
    }
    CHECK_MSG(saw_brake, "Stock braking never reached physical input");
    CHECK_MSG(std::fabs(physics.riding.motion.ground_speed) < speed_before_brake,
              "Braking did not reduce the moving board's speed");
    CHECK(physics.riding.ground.wheel_contact_count > 0);
}

// #[ignore = "requires private stock skater, animation banks and collections"]
S3_TEST(customiser_equipment_reaches_ground_force_and_torque) {
    const auto* vfs = pack();
    if (!vfs) return;
    const std::string& root = s3test::game::ASSET_ROOT;
    auto manifest = s3::data::GameAssets::load(*vfs, root);
    CHECK_MSG(manifest.ok, manifest.error);
    auto graphs = graph_runtime::StockGraphs::load(*vfs, root, manifest.value).unwrap();
    std::vector<std::pair<float, float>> samples;
    for (const float hardness : {0.0f, 0.7f, 1.0f}) {
        auto physics = GamePhysics::load(*vfs, root).unwrap();
        auto skater = SkaterRuntime::load(*vfs, root, graphs, physics, "normal").unwrap();
        Preferences profile{};
        profile.truck = static_cast<double>(hardness);
        profile.wheel = static_cast<double>(hardness);
        apply_preferences(profile, physics, skater.animation);
        PlayerControls controls{};
        auto camera = camera::CameraRuntime::load(*vfs, root).unwrap();
        const input::controllers::ControllerInput input{};
        {
            auto actions = input.player_actions();
            const auto advanced = frame::advance(physics, skater, controls, graphs, actions, false, camera);
            CHECK_MSG(advanced.is_ok(), err_text(advanced));
        }
        // Replay identical observed sideways travel at the Ground input boundary.
        // Only the published profile input changes; run the actual Ground adapter.
        auto& p = skater.player_input.processed;
        CHECK_EQ(p.scalar_2764, hardness);
        CHECK_EQ(p.truck_tightness_2760, hardness);
        const std::array<std::uint32_t, 4> travel{s3::rs::to_bits(1.0f), s3::rs::to_bits(0.0f),
                                                  s3::rs::to_bits(3.0f), s3::rs::to_bits(0.0f)};
        p.vectors_400_416 = {travel, travel};
        p.scalar_2656 = s3::rs::sqrt(10.0f);
        skater.ground.state.elapsed_2648 = 0.25f;
        physics.board.forces_mut().clear();
        auto outcome = ground_phase::advance(physics, skater);
        CHECK_MSG(outcome.is_ok(), (outcome.is_ok() ? std::string() : outcome.unwrap_err()));
        CHECK(std::holds_alternative<s3::riding::grounded::state::board::GroundBoardOutcome::Ordinary>(
            outcome.unwrap().value));
        const auto entries = physics.board.forces().entries();
        const s3::physics::force_queue::QueuedPointForce* side = nullptr;
        for (std::size_t i = 0; i < entries.size(); ++i) {
            if (entries[i].tag == 1) {
                side = &entries[i];
                break;
            }
        }
        CHECK(side != nullptr);
        samples.emplace_back(side->force_world.x, physics.board.bodies()[6].rates.torque_acceleration.y);
    }
    // Original82D92AD4 multiplies side friction by lerp(softest,1,hardness).
    auto data = s3::data::Collections::load(*vfs, root);
    CHECK_MSG(data.ok, data.error);
    auto softest_result = data.value.float_("physicswheels", "default", "SoftestWheelSideFrictionScalar");
    CHECK_MSG(softest_result.ok, softest_result.error);
    const float softest = softest_result.value;
    CHECK_MSG(std::fabs(samples[2].first) > 0.001f, "Sideways travel produced no side force");
    const std::array<float, 3> hardnesses{0.0f, 0.7f, 1.0f};
    for (std::size_t index = 0; index < 3; ++index) {
        const float expected = std::fma(1.0f - softest, hardnesses[index], softest);
        CHECK_MSG(std::fabs(samples[index].first / samples[2].first - expected) < 0.0001f,
                  "Profile hardness was lost before live friction");
    }
    // Original82D92BBC also changes straighten duration, reaching deck torque.
    CHECK_MSG(std::fabs(samples[0].second - samples[2].second) > 0.001f,
              "Profile hardness was lost before live straighten torque");
    for (const auto& s : samples) {
        std::fprintf(stderr, "Ground wheel-hardness force/torque sample: (%g, %g)\n", static_cast<double>(s.first),
                     static_cast<double>(s.second));
    }
}

// #[ignore = "requires the user's converted stock collection assets"]
S3_TEST(stock_geometry_loads_and_reaches_the_live_board_solver) {
    const auto* vfs = s3test::game::assets();
    if (!vfs) return;
    auto loaded = GamePhysics::load(*vfs, s3test::game::ASSET_ROOT);
    CHECK_MSG(loaded.is_ok(), (loaded.is_ok() ? std::string() : loaded.unwrap_err()));
    auto& physics = loaded.unwrap();
    const auto volumes = colliders::world_volumes(physics.board, physics.settings);
    CHECK_EQ(volumes.size(), 21u);  // Four wheels, two trucks, 15 stock deck children.
    for (int i = 0; i < 120; ++i) CHECK(physics.advance_board().is_ok());
    CHECK(physics.contact_count > 0);
    const auto reports = physics.board.contact_reports();
    CHECK_MSG(!reports.empty(), "actual solved contacts must reach gameplay feedback");
    CHECK(reports.size() <= 16);
    bool wheel = false;
    bool finite = true;
    for (const auto& report : reports) {
        wheel = wheel || s3::physics::board::index(report.part) < 4;
        finite = finite && s3::rs::is_finite(report.normal_force_on_a.y);
    }
    CHECK(wheel);
    CHECK(finite);
    CHECK_EQ(physics.ticks, 120u);
    const auto deck = physics.board.part_transforms()[s3::physics::board::index(BodyId::Deck)];
    CHECK(deck.translation.y > ground::HEIGHT);
}

// #[ignore = "requires private stock animation banks and collections; no window or renderer"]
S3_TEST(customiser_styles_and_postures_change_stock_pose) {
    using s3::animation::playback::PlayAnimation;
    using s3::animation::playback::PlayAnimationInstance;
    using s3::animation::playback::TransitionSettings;
    using s3::animation::playback_tree::Evaluation;
    using s3::animation::playback_tree::PoseCommand;
    const auto* vfs = pack();
    if (!vfs) return;
    const std::string& root = s3test::game::ASSET_ROOT;
    auto manifest = s3::data::GameAssets::load(*vfs, root);
    CHECK_MSG(manifest.ok, manifest.error);
    auto graphs = graph_runtime::StockGraphs::load(*vfs, root, manifest.value).unwrap();
    auto physics = GamePhysics::load(*vfs, root).unwrap();
    auto skater = SkaterRuntime::load(*vfs, root, graphs, physics, "normal").unwrap();
    PlayAnimation operation{};
    operation.animation = "S_R_PUSHLSP_HSTR_N_0_CYC1";
    operation.switch_animation = std::nullopt;
    operation.mirror_animation = std::nullopt;
    operation.no_board_animation = std::nullopt;
    operation.playback_speed = 1.0f;
    operation.apply_posture = true;
    operation.transition = TransitionSettings{1, 0.0f, 0, 0, false};
    operation.parameters = {};
    std::vector<std::vector<s3::animation::output::Sqt>> samples;
    const std::array<std::pair<int, int>, 7> cases{{{0, 0}, {1, 0}, {2, 0}, {3, 0}, {0, 1}, {0, 2}, {0, 3}}};
    for (const auto& [style, posture] : cases) {
        Preferences profile{};
        profile.style = style;
        profile.posture = posture;
        apply_preferences(profile, physics, skater.animation);
        auto& motion = skater.animation.motion;
        CHECK(PlayAnimationInstance{}.begin(operation, motion.playback_context, motion.animation).is_ok());
        CHECK(motion.animation.apply_parameters().is_ok());
        motion.animation.seek_current_fraction(0.35f);
        const auto commands = motion.animation.evaluate_pose(Evaluation{0.0001f, false}).unwrap();
        const auto pose = skater.animation.evaluator->evaluate(commands).unwrap();
        std::string names;
        for (const auto& c : commands) {
            if (const auto* clip = std::get_if<PoseCommand::Clip>(&c.value)) {
                names += "\"" + clip->name + "\", ";
            } else if (const auto* p = std::get_if<PoseCommand::Pose>(&c.value)) {
                names += "\"" + p->name + "\", ";
            }
        }
        std::fprintf(stderr, "Style %d, posture %d: [%s]\n", style, posture, names.c_str());
        if (!samples.empty()) {
            CHECK_MSG(pose != samples.front(),
                      "Style " << style << ", posture " << posture << " did not change the evaluated stock pose");
        }
        samples.push_back(pose);
    }
    skater.animation.set_customisation(1, 0);
    const auto stance = skater.animation.stance();
    skater.animation.set_customisation(0, 0);
    CHECK(skater.animation.stance() != stance);
    skater.animation.set_customisation(1, 0);
    CHECK(skater.animation.stance() == stance);
}

// #[ignore = "requires private stock assets"]
S3_TEST(raw_y_reaches_stock_dismount) {
    const auto* vfs = pack();
    if (!vfs) return;
    const std::string& root = s3test::game::ASSET_ROOT;
    auto assets = s3::data::GameAssets::load(*vfs, root);
    CHECK_MSG(assets.ok, assets.error);
    auto graphs = graph_runtime::StockGraphs::load(*vfs, root, assets.value).unwrap();
    auto physics = GamePhysics::load(*vfs, root).unwrap();
    auto skater = SkaterRuntime::load(*vfs, root, graphs, physics, "normal").unwrap();
    PlayerControls controls{};
    input::controllers::ControllerInput input{};
    auto camera = camera::CameraRuntime::load(*vfs, root).unwrap();
    bool saw_toggle = false;
    std::optional<int> first_offboard_tick;
    auto previous_state = skater.player_state.current();
    std::optional<std::array<float, 4>> walk_start, walk_end, camera_start, camera_end;
    bool held_board_seen = false;
    bool released_board_seen = false;
    for (int tick = 0; tick < 2400; ++tick) {
        std::array<std::int16_t, 2> left{0, 0};
        if (tick >= 120 && tick < 300) {
            left = {0, 24000};
        } else if (tick >= 800 && tick < 920) {
            left = {24000, 0};
        } else if (tick >= 920 && tick < 1040) {
            left = {-24000, 0};
        }
        input.sample_raw_for_test(xbox(tick == 20 ? 0x8000 : 0,
                                       (tick >= 400 && tick < 460) ? std::array<std::uint8_t, 2>{0, 255}
                                                                   : std::array<std::uint8_t, 2>{0, 0},
                                       left));
        auto actions = input.player_actions();
        {
            const auto updated = controls.update_for_physics(actions, physics, skater, camera);
            CHECK_MSG(updated.is_ok(), err_text(updated));
        }
        const std::uint32_t published_slope = skater.player_input.physical.off_board.kind_88;
        const bool published_thin = skater.player_input.physical.off_board.flag_330 != 0;
        const auto result = frame::advance(physics, skater, controls, graphs, actions, true, camera);
        saw_toggle = saw_toggle || skater.animation.motion.action_intents.get("NewToggleOffBoardState") != nullptr;
        CHECK_MSG(result.is_ok(), "Y tick" << tick << ", input_received=" << saw_toggle << ", state="
                                           << static_cast<std::uint32_t>(skater.player_state.current()) << ": "
                                           << err_text(result));
        CHECK_MSG(skater.animation.motion.ground_slope_type == std::optional<std::uint32_t>(published_slope),
                  "GroundSlopeType did not read completed OffBoard88 at tick" << tick);
        CHECK_MSG(skater.animation.motion.biped_ground_thin == std::optional<bool>(published_thin),
                  "IsBipedGroundThin did not read completed OffBoard330 at tick" << tick);
        const auto root_position = skater.animated_skeleton.roots.animation_to_world[3];
        held_board_seen = held_board_seen || skater.skateboard_controller.fields.state_448 == 1;
        if (held_board_seen && skater.skateboard_controller.fields.state_448 == 2) {
            released_board_seen = true;
            //LetGo82D75440 clears both deck-drive channels and the selected
            //hand drive before entering released state2. Test the live owners.
            const std::array<std::uint32_t, 8> released{0, 0, 0, 2, 0, 0, 0, 2};
            CHECK_MSG(physics.board.hook().drive.dynamics == released,
                      "Deck remains driven after release tick" << tick);
            CHECK_MSG(skater.board_possession.state.selected_hand_424 == 2u,
                      "Released board retains a hand owner at tick" << tick);
            const std::array<std::array<std::uint32_t, 4>, 2> idle{{{0, 0, 0, 2}, {0, 0, 0, 2}}};
            for (const auto& hand : skater.board_possession.state.hands) {
                CHECK_MSG(hand.dynamics == idle, "Hand drive remains armed after release tick" << tick);
            }
        }
        CHECK_MSG(camera.frame.has_value(), "Missing gameplay camera");
        const auto camera_position = camera.frame->position;
        bool camera_finite = true;
        for (const float v : camera_position) camera_finite = camera_finite && std::isfinite(v);
        CHECK_MSG(camera_finite, "Camera tick" << tick);
        bool pose_finite = true;
        for (const auto& m : skater.render_pose) {
            for (const auto& row : m) {
                for (const float v : row) pose_finite = pose_finite && std::isfinite(v);
            }
        }
        CHECK_MSG(pose_finite, "Non-finite rendered skeleton at tick" << tick);
        {
            std::array<float, 4> published{};
            for (std::size_t i = 0; i < 4; ++i) {
                published[i] = s3::rs::from_bits(skater.player_input.physical.reckoning.vector_64[i]);
            }
            CHECK_MSG(published == skater.animated_skeleton.board_frames.centre_of_mass,
                      "COM output was overwritten after skeleton publication at tick" << tick);
        }
        std::array<float, 4> camera4{};
        for (std::size_t i = 0; i < 4; ++i) camera4[i] = camera_position[i];
        if (tick == 120) {
            walk_start = root_position;
            camera_start = camera4;
        }
        if (tick == 299) {
            walk_end = root_position;
            camera_end = camera4;
        }
        if (tick == 240) {
            const auto attrs = skater.animation_input.extra;
            CHECK_MSG(attrs.offboard_magnitude > 0.1f, "Stock AG/MG did not produce offboard magnitude");
            CHECK_MSG(std::fabs(attrs.biped_world_x) + std::fabs(attrs.biped_world_z) > 0.1f,
                      "Stock AG/MG did not publish Biped world direction");
        }
        if (tick >= 120 && tick < 2400) {
            CHECK_MSG(skater.player_state.current() == PhysicalStateId::BipedGround,
                      "Ordinary walking/trigger input left BipedGround at tick" << tick);
            // Measure the actual shared-solver joint anchors, not a synthetic
            // stand pose. A 35cm separation is visible limb failure, not
            // normal constraint compliance. This is an acceptance threshold.
            const auto& bodies = skater.skeleton.bodies();
            auto anchor = [&](std::size_t part, const std::uint32_t* words) {
                const auto& body = bodies[part];
                std::array<float, 3> local{};
                for (int i = 0; i < 3; ++i) local[i] = s3::rs::from_bits(words[i]);
                const auto& basis = body.rates.basis.columns;
                const std::array<float, 3> position{body.rates.position.x, body.rates.position.y,
                                                    body.rates.position.z};
                std::array<float, 3> out{};
                for (int i = 0; i < 3; ++i) {
                    out[i] = position[i] + basis[0][i] * local[0] + basis[1][i] * local[1] + basis[2][i] * local[2];
                }
                return out;
            };
            for (const auto& joint : skater.skeleton_joints.records) {
                const auto child = anchor(joint.child, &joint.frames.words[4]);
                const auto parent = anchor(joint.parent, &joint.frames.words[12]);
                float sum = -0.0f;
                for (int i = 0; i < 3; ++i) sum += (child[i] - parent[i]) * (child[i] - parent[i]);
                const float gap = s3::rs::sqrt(sum);
                CHECK_MSG(gap < 0.35f, "Broken joint at tick" << tick << ": " << joint.parent << " -> " << joint.child
                                                              << ", gap=" << gap << "; board_state="
                                                              << skater.skateboard_controller.fields.state_448);
            }
        }
        if (skater.player_state.current() != previous_state || tick % 120 == 0) {
            std::fprintf(stderr, "dismount tick=%d state=%u root=(%g, %g, %g)\n", tick,
                         static_cast<std::uint32_t>(skater.player_state.current()),
                         static_cast<double>(root_position[0]), static_cast<double>(root_position[1]),
                         static_cast<double>(root_position[2]));
            previous_state = skater.player_state.current();
        }
        if (s3::player::state::category(skater.player_state.current()) == 500) {
            CHECK(saw_toggle);
            if (!first_offboard_tick) first_offboard_tick = tick;
        }
    }
    CHECK_MSG(first_offboard_tick.has_value(), "Y did not reach off-board physics; input_received=" << saw_toggle);
    CHECK_MSG(held_board_seen && released_board_seen, "Replay did not exercise held-to-released board possession");
    auto distance = [](std::array<float, 4> a, std::array<float, 4> b) {
        return s3::rs::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[2] - b[2]) * (a[2] - b[2]));
    };
    // Acceptance bounds for this recorded three-second input, not gameplay clamps.
    const float walked = distance(walk_start.value(), walk_end.value());
    CHECK_MSG(walked > 1.0f && walked < 20.0f, "Implausible walking displacement: " << walked);
    const float followed = distance(camera_start.value(), camera_end.value());
    CHECK_MSG(followed > 1.0f && followed < 20.0f, "Camera did not follow walking: " << followed);
}
