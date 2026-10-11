// Port of skate-game/src/physics/climbing/tests.rs (mounted as climbing::tests) and the test in
// physics/climbing/approach.rs. The `#[ignore = "requires private stock graphs and authored climbing
// clips"]` tests run on the cleanroom pack (skip without it). The authored clips
// (`private/custom/climbing.json`) are an optional extension asset: where Rust would panic on the
// missing clips (`clips.as_ref().unwrap()`), the C++ test reports a skip instead.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "game_test_assets.hpp"
#include "harness.hpp"
#include "s3/core/input/xbox.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/collision.hpp"
#include "s3/core/physics/contact.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/data/manifest.hpp"
#include "s3/game/camera.hpp"
#include "s3/game/difficulty.hpp"
#include "s3/game/glam_ext.hpp"
#include "s3/game/graph_runtime.hpp"
#include "s3/game/input/controllers.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/climbing.hpp"
#include "s3/game/physics/controls.hpp"
#include "s3/game/physics/frame.hpp"
#include "s3/game/physics/ground.hpp"
#include "s3/game/physics/player_state.hpp"
#include "s3/game/physics/skater.hpp"

using namespace s3::game;
using namespace s3::game::physics;
using namespace s3::game::physics::climbing;
using s3::game::glam::Mat4;
using s3::game::glam::Vec3;
using s3::game::glam::Vec4;
using s3::math::Vector3;
using s3::physics::board_world::BoardWorld;
using s3::physics::board_world::WorldTriangle;
using s3::physics::board_world::query_metadata::Bounds;
using s3::physics::board_world::query_metadata::QueryMesh;
using s3::physics::board_world::query_metadata::QueryMetadata;
using s3::physics::board_world::query_metadata::QueryPool;
using s3::physics::board_world::query_metadata::Range;
using s3::physics::collision::TriangleFeature;
using s3::physics::contact::RetailContactMaterial;
using s3::physics::drive_frames::RetailAffineTransform;
using s3::player::state::PhysicalStateId;

namespace {

BoardWorld world(float height, float width, float depth, bool ceiling, bool top) {
    std::vector<WorldTriangle> triangles;
    auto quad = [&](std::array<std::array<float, 3>, 4> p) {
        for (const auto& ids : {std::array<int, 3>{0, 1, 2}, std::array<int, 3>{0, 2, 3}}) {
            std::array<Vector3, 3> vertices{};
            for (int k = 0; k < 3; ++k) vertices[k] = Vector3{p[ids[k]][0], p[ids[k]][1], p[ids[k]][2]};
            triangles.push_back(s3::rs::unwrap(WorldTriangle::from_vertices(
                vertices, RetailContactMaterial{0.8f, 0.6f, 0.0f}, 0, TriangleFeature::ONE_SIDED,
                {1.0f, 1.0f, 1.0f}, 0.0f)));
        }
    };
    quad({{{-20.0f, 0.0f, -20.0f}, {-20.0f, 0.0f, 20.0f}, {20.0f, 0.0f, 20.0f}, {20.0f, 0.0f, -20.0f}}});
    quad({{{-width, 0.0f, 0.6f}, {-width, height, 0.6f}, {width, height, 0.6f}, {width, 0.0f, 0.6f}}});
    if (top) {
        quad({{{-width, height, 0.6f},
               {-width, height, 0.6f + depth},
               {width, height, 0.6f + depth},
               {width, height, 0.6f}}});
    }
    if (ceiling) {
        quad({{{-width, height + 1.2f, 0.3f},
               {width, height + 1.2f, 0.3f},
               {width, height + 1.2f, 3.0f},
               {-width, height + 1.2f, 3.0f}}});
    }
    const std::size_t count = triangles.size();
    QueryMetadata metadata{};
    metadata.packed_surfaces = std::vector<std::uint16_t>(count, 0);
    metadata.static_edges = {};
    metadata.island_flags = 0;
    QueryMesh mesh{};
    mesh.geometry = 1;
    mesh.rejection_flags = 0;
    mesh.triangle_range = Range{0, count};
    mesh.pool = QueryPool::Ground;
    mesh.matching_group = 0;
    mesh.local_bounds = Bounds{Vector3{-20.0f, -1.0f, -20.0f}, Vector3{20.0f, height + 2.0f, 20.0f}};
    mesh.local_to_world = RetailAffineTransform::IDENTITY;
    mesh.world_to_local = RetailAffineTransform::IDENTITY;
    metadata.meshes = {mesh};
    return BoardWorld::with_query_metadata(std::move(triangles), std::move(metadata)).unwrap();
}

Mat4 mat4_from_translation(Vec3 t) {
    return Mat4::from_cols(Vec4{1.0f, 0.0f, 0.0f, 0.0f}, Vec4{0.0f, 1.0f, 0.0f, 0.0f}, Vec4{0.0f, 0.0f, 1.0f, 0.0f},
                           t.extend(1.0f));
}

s3::input::xbox::XboxState xbox(std::uint16_t buttons, std::array<std::uint8_t, 2> triggers,
                                std::array<std::int16_t, 2> left) {
    s3::input::xbox::XboxState state{};
    state.buttons = buttons;
    state.triggers = triggers;
    state.left = left;
    state.right = {0, 0};
    return state;
}

std::string err_text(const s3::game::Status& s) { return s.is_ok() ? std::string() : s.unwrap_err(); }

/// `capture`: writes a JSON pose snapshot when SKATE_CLIMB_CAPTURE is set. The serde_json
/// dump is a debugging aid only (no assertion); here it reports the request instead.
void capture(const SkaterRuntime& skater, const std::string& name) {
    if (skater.skateboard_controller.fields.state_448 != 1) return;
    if (std::getenv("SKATE_CLIMB_CAPTURE") != nullptr) {
        std::fprintf(stderr, "climb capture %s: JSON capture not supported in the C++ port\n", name.c_str());
    }
}

void run_climb(bool drop_board, bool miss) {
    const auto* vfs = s3test::game::assets();
    if (vfs == nullptr) return;
    const std::string& root = s3test::game::ASSET_ROOT;
    auto assets = s3::data::GameAssets::load(*vfs, root);
    CHECK_MSG(assets.ok, assets.error);
    auto graphs = graph_runtime::StockGraphs::load(*vfs, root, assets.value).unwrap();
    auto physics = GamePhysics::load_with_difficulty(*vfs, root, nullptr, difficulty::Difficulty::Normal).unwrap();
    auto skater = SkaterRuntime::load(*vfs, root, graphs, physics, "normal").unwrap();
    auto camera = camera::CameraRuntime::load(*vfs, root).unwrap();
    PlayerControls controls{};
    input::controllers::ControllerInput input{};
    auto frame_advance = [&](bool available) {
        auto actions = input.player_actions();
        const auto advanced = frame::advance(physics, skater, controls, graphs, actions, available, camera);
        CHECK_MSG(advanced.is_ok(), err_text(advanced));
    };
    auto controls_update = [&]() {
        auto actions = input.player_actions();
        controls.update(actions, physics.settings.step.simulation.time_step,
                        physics.settings.input_magnitude_threshold,
                        skater.player_input.physical.scoring.capabilities_204);
    };
    // Dismount through the real stock controller and graphs before testing.
    for (int tick = 0; tick < 360; ++tick) {
        input.sample_raw_for_test(xbox(tick == 120 ? 0x8000 : 0,
                                       (drop_board && tick == 180) ? std::array<std::uint8_t, 2>{255, 0}
                                                                   : std::array<std::uint8_t, 2>{0, 0},
                                       {0, 0}));
        controls_update();
        frame_advance(true);
    }
    CHECK_EQ(skater.player_state.current(), PhysicalStateId::BipedGround);
    CHECK_EQ(skater.skateboard_controller.fields.state_448 == 1, !drop_board);
    if (!skater.climbing.clips.has_value()) {
        std::printf("  (skip: no authored climbing clips in the pack)\n");
        return;
    }
    physics.world = world(2.2f, 4.0f, 8.0f, false, true);
    // Put the real skeleton's feet at the fixture origin before entering Biped.
    {
        const auto& c = skater.climbing.clips->reach;
        std::vector<Mat4> g;
        for (const std::size_t i : skater.climbing.indices) g.push_back(matrix(skater.render_pose[i]));
        const Mat4 root_matrix = mat4_from_translation(-c.feet(g) - glam::VEC3_Z * 2.0f);
        auto pose = skater.render_pose;
        CHECK(publish_pose(physics, skater, root_matrix, pose).is_ok());
    }
    CHECK(player_state::resume_after_climb(physics, skater).is_ok());
    controls.offboard_direction = std::array<float, 4>{0.0f, 0.0f, 1.0f, 0.0f};
    auto button = [](PlayerControls& c, bool down, bool previous) {
        auto words = c.controller.words();
        words[13] = static_cast<std::uint32_t>(down) << 23;
        words[6] = static_cast<std::uint32_t>(previous) << 23;
        c.controller = s3::input::controller::DerivedControllerInput::from_words(words);
    };
    approach::advance(physics, skater, controls);
    bool saw_air = false;
    bool saw_reach = false;
    bool saw_ground_reach = false;
    for (int tick = 0; tick < 180; ++tick) {
        input.sample_raw_for_test(xbox((tick >= 33 && tick < 120) ? 0x4000 : 0, {0, 0}, {0, 24000}));
        controls_update();
        controls.offboard_direction = std::array<float, 4>{0.0f, 0.0f, (miss && tick >= 45) ? -1.0f : 1.0f, 0.0f};
        frame_advance(true);
        saw_air = saw_air || skater.player_state.current() == PhysicalStateId::BipedAir;
        saw_reach = saw_reach || skater.climbing.approach.has_value();
        saw_ground_reach = saw_ground_reach || (skater.player_state.current() == PhysicalStateId::BipedGround &&
                                                skater.climbing.approach.has_value() &&
                                                skater.climbing.approach->weight > 0.02f);
        const std::array<int, 8> shots{15, 25, 30, 35, 40, 45, 50, 55};
        if (!miss && std::find(shots.begin(), shots.end(), tick) != shots.end()) {
            capture(skater, "00_jump_" + std::to_string(tick));
        }
        if (skater.climbing.active.has_value() && skater.climbing.active->phase == Phase::Hang) break;
    }
    CHECK_MSG(saw_air, "must use stock airborne jump before catching");
    CHECK_MSG(saw_ground_reach, "arms must start reaching during the run");
    if (miss) {
        CHECK_MSG(saw_reach, "miss scenario must begin a reach first");
        CHECK(!skater.climbing.active.has_value());
        CHECK(!skater.climbing.approach.has_value());
        CHECK_EQ(skater.player_state.current(), PhysicalStateId::BipedGround);
        return;
    }
    CHECK_MSG(skater.climbing.active.has_value(), "stock jump failed to catch");
    CHECK_EQ(skater.climbing.active->phase, Phase::Hang);
    {
        const auto& c = skater.climbing.clips->reach;
        std::vector<Mat4> g;
        for (const std::size_t i : skater.climbing.indices) g.push_back(matrix(skater.render_pose[i]));
        const auto ledge = skater.climbing.active->ledge;
        const Mat4 root = matrix(skater.animated_skeleton.roots.animation_to_world);
        const std::array<const char*, 2> names{"LEFTHAND", "RIGHTHAND"};
        for (std::size_t i = 0; i < 2; ++i) {
            const std::string name = names[i];
            const Mat4 hand = root * g[c.index(name)];
            const Vec3 palm = glam::transform_point3(hand, contacts::PALM);
            CHECK_MSG(glam::distance(palm, ledge.palms[i]) < 0.015f, name << ": palm too far from target");
            CHECK(glam::truncate(hand.y_axis).dot(-ledge.normals[i]) > 0.99f);
            const Mat4 lower = g[c.index(i == 0 ? "LEFTFOREARM" : "RIGHTFOREARM")];
            const auto relative = Transform::from_matrix(lower.inverse() * g[c.index(name)]);
            const Vec3 axis = glam::normalize(relative.translation);
            const auto q = relative.rotation;
            CHECK_MSG(std::fabs(Vec3::new_(q.x, q.y, q.z).dot(axis)) < 0.01f,
                      "forearm roll must not invert the wrist: " << name);
        }
        const Vec3 hips = glam::transform_point3(matrix(skater.animated_skeleton.roots.animation_to_world),
                                                 glam::truncate(g[c.index("HIPS")].w_axis));
        CHECK_MSG(hips.z < 0.45f, "hanging hips must stay outside the wall: " << hips.z);
    }
    capture(skater, "02_hanging");
    auto climb_advance = [&]() -> bool {
        auto advanced = climbing::advance(physics, skater, controls, camera);
        CHECK_MSG(advanced.is_ok(), (advanced.is_ok() ? std::string() : advanced.unwrap_err()));
        return advanced.unwrap();
    };
    button(controls, false, true);
    climb_advance();
    button(controls, true, false);
    climb_advance();
    CHECK_EQ(skater.climbing.active.value().phase, Phase::Mantle);
    button(controls, false, true);
    for (int tick = 0; tick < 260; ++tick) {
        if (!climb_advance()) break;
        bool finite = true;
        for (const auto& m : skater.render_pose) {
            for (const auto& row : m) {
                for (const float v : row) finite = finite && std::isfinite(v);
            }
        }
        CHECK(finite);
        if (tick == 30 || tick == 90 || tick == 150 || tick == 210) {
            capture(skater, "03_mantle_" + std::to_string(tick));
        }
    }
    CHECK(!skater.climbing.active.has_value());
    CHECK_EQ(skater.player_state.current(), PhysicalStateId::BipedGround);
    const Vec3 landing = glam::truncate(matrix(skater.animated_skeleton.roots.animation_to_world).w_axis);
    capture(skater, "04_handoff");
    input.sample_raw_for_test(xbox(0, {0, 0}, {0, 0}));
    controls.offboard_direction = std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f};
    for (int tick = 0; tick < 120; ++tick) {
        controls_update();
        frame_advance(false);
        CHECK_MSG(skater.player_state.current() == PhysicalStateId::BipedGround,
                  "unexpected state after handoff at tick " << tick);
    }
    const Vec3 end = glam::truncate(matrix(skater.animated_skeleton.roots.animation_to_world).w_axis);
    CHECK_MSG(glam::distance(end, landing) < 0.1f, "handoff drifted");
    CHECK_EQ(skater.player_state.current(), PhysicalStateId::BipedGround);
    capture(skater, "05_standing");
    for (int i = 0; i < 60; ++i) {
        input.sample_raw_for_test(xbox(0, {0, 0}, {0, 16000}));
        controls_update();
        controls.offboard_direction = std::array<float, 4>{0.0f, 0.0f, 0.5f, 0.0f};
        frame_advance(true);
    }
    const Vec3 walked = glam::truncate(matrix(skater.animated_skeleton.roots.animation_to_world).w_axis);
    CHECK_MSG(walked.z > end.z + 0.2f, "walking did not resume: " << end.z << " -> " << walked.z);
    CHECK_MSG(std::fabs(walked.y - 2.2f) < 0.15f, "walking left the ledge: " << walked.y);
    CHECK_EQ(skater.player_state.current(), PhysicalStateId::BipedGround);
}

}  // namespace

S3_TEST(reachable_ledge_needs_width_support_and_headroom) {
    auto probe = [](const BoardWorld& w) { return ledge::find(w, glam::VEC3_ZERO, glam::VEC3_Z); };
    const auto good = probe(world(2.2f, 2.0f, 3.0f, false, true));
    CHECK_MSG(good.has_value(), "reachable ledge");
    CHECK(std::fabs(good->anchor.y - 2.225f) < 0.002f);
    CHECK_MSG(!probe(world(4.0f, 2.0f, 3.0f, false, true)).has_value(), "too high");
    CHECK_MSG(!probe(world(2.2f, 2.0f, 3.0f, false, false)).has_value(), "no top");
    CHECK_MSG(!probe(world(2.2f, 0.2f, 3.0f, false, true)).has_value(), "too narrow for hands");
    CHECK_MSG(!probe(world(2.2f, 2.0f, 0.4f, false, true)).has_value(), "no standing support");
    CHECK_MSG(!probe(world(2.2f, 2.0f, 3.0f, true, true)).has_value(), "ceiling");
    CHECK(!ledge::find(world(2.2f, 2.0f, 3.0f, false, true), glam::VEC3_ZERO, -glam::VEC3_Z).has_value());
}

S3_TEST(default_course_has_a_reachable_test_block) {
    const auto w = ground::world(RetailContactMaterial{0.8f, 0.6f, 0.0f});
    CHECK(ledge::find(w, Vec3::new_(-4.4f, ground::FLOOR_HEIGHT, 0.0f), -glam::VEC3_X).has_value());
}

// #[ignore = "requires private stock graphs and authored climbing clips"]
S3_TEST(hybrid_climb_holds_for_second_press_and_resumes_walking) { run_climb(false, false); }

// #[ignore = "requires private stock graphs and authored climbing clips"]
S3_TEST(climbing_with_a_dropped_board_preserves_free_board_physics) { run_climb(true, false); }

// #[ignore = "requires private stock graphs and authored climbing clips"]
S3_TEST(turning_away_during_reach_fades_out_and_lands_normally) { run_climb(false, true); }

// ---- physics/climbing/approach.rs ---------------------------------------------------------

S3_TEST(reach_builds_continuously_before_takeoff) {
    using approach::reach_gain;
    CHECK_EQ(reach_gain(2.6f, false), 0.0f);
    CHECK(reach_gain(2.0f, false) > 0.0f);
    CHECK(reach_gain(1.5f, false) > reach_gain(2.0f, false));
    CHECK(reach_gain(0.8f, false) <= 0.45f);
    CHECK_EQ(reach_gain(0.8f, true), 1.0f);
    for (int i = 1; i < 260; ++i) {
        const float x = static_cast<float>(i) * 0.01f;
        CHECK(std::fabs(reach_gain(x, true) - reach_gain(x + 0.01f, true)) < 0.009f);
    }
}
