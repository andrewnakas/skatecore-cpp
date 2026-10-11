// Port of skate-game/src/tests/map_startup.rs (mounted in physics.rs as `map_startup`).
//! Production startup/frame integration, following physics_startup.rs.
//! Private inputs are loaded at runtime; these checks do not establish TU3 parity.
// Both Rust tests are `#[ignore]` private-asset tests; here they run on the cleanroom pack (skip
// without it). The Rust demo map (`include_bytes!("maps/format-demo.skate")`) is not part of the
// pack, so `demo()` reads the pack's KenneyPark.skate (s3test::game::map_path(), override S3_MAP);
// the private extracted map (SKATE_MAP_TEST_PATH) defaults to the same file.
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "game_test_assets.hpp"
#include "harness.hpp"
#include "s3/core/input/xbox.hpp"
#include "s3/core/physics/board.hpp"
#include "s3/data/manifest.hpp"
#include "s3/data/skate_map.hpp"
#include "s3/game/camera.hpp"
#include "s3/game/graph_runtime.hpp"
#include "s3/game/input/controllers.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/controls.hpp"
#include "s3/game/physics/frame.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/skate_world.hpp"

using namespace s3::game;
using namespace s3::game::physics;
using s3::data::SkateMap;
using s3::math::Vector3;
using s3::physics::board::BodyId;

namespace {

constexpr std::uint64_t TICKS = 24;
// Numerical assertion tolerances, not simulation settings.
constexpr float POSE_EPSILON = 0.002f;
constexpr std::array<float, 3> OFFSET{137.0f, 7.0f, -83.0f};
using F3 = std::array<float, 3>;

std::string err_text(const s3::game::Status& s) { return s.is_ok() ? std::string() : s.unwrap_err(); }

bool load_map(const std::string& path, SkateMap& out) {
    std::vector<std::uint8_t> bytes;
    if (!s3test::game::read_file(path, bytes)) {
        std::printf("  (skip: no map at %s)\n", path.c_str());
        return false;
    }
    auto parsed = SkateMap::parse(bytes.data(), bytes.size());
    CHECK_MSG(parsed.ok, path << ": " << parsed.error);
    out = std::move(parsed.value);
    return true;
}

// Independent +90 degree Y rotation: catches a reversed heading convention.
F3 rotate(F3 p) { return {p[2], p[1], -p[0]}; }

F3 relocate(F3 point) {
    const F3 rotated = rotate(point);
    F3 out{};
    for (std::size_t i = 0; i < 3; ++i) out[i] = rotated[i] + OFFSET[i];
    return out;
}

F3 xyz(Vector3 v) { return {v.x, v.y, v.z}; }
F3 head3(const std::array<float, 4>& v) { return {v[0], v[1], v[2]}; }

void close(F3 actual, F3 expected, const char* context) {
    bool ok = true;
    for (std::size_t i = 0; i < 3; ++i) ok = ok && std::fabs(actual[i] - expected[i]) < POSE_EPSILON;
    CHECK_MSG(ok, context << ": actual=[" << actual[0] << ", " << actual[1] << ", " << actual[2] << "], expected=["
                          << expected[0] << ", " << expected[1] << ", " << expected[2] << "]");
}

bool all_finite(F3 v) { return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]); }

struct Gameplay {
    GamePhysics physics;
    SkaterRuntime skater;
    PlayerControls controls;
    input::controllers::ControllerInput input;
    camera::CameraRuntime camera;

    static Gameplay load(const s3::data::Vfs& vfs, const std::string& root, const graph_runtime::StockGraphs& graphs,
                         const SkateMap& map) {
        const auto valid = skate_world::validate_runtime(map);
        CHECK_MSG(valid.is_ok(), err_text(valid));
        // Deliberately pass Course: Some(map) must supersede the terrain.
        auto physics = GamePhysics::load_with_world(vfs, root, ground::Terrain::Course, &map);
        CHECK_MSG(physics.is_ok(),
                  "Map \"" << map.name << "\" physics startup: " << (physics.is_ok() ? "" : physics.unwrap_err()));
        auto skater = SkaterRuntime::load(vfs, root, graphs, physics.unwrap(), "normal").unwrap();
        return Gameplay{std::move(physics.unwrap()), std::move(skater), PlayerControls{},
                        input::controllers::ControllerInput{}, camera::CameraRuntime::load(vfs, root).unwrap()};
    }

    void advance(const graph_runtime::StockGraphs& graphs) {
        // A connected, neutral controller exercises the normal graph/input path.
        // No forced state, velocity, contact, camera or animation overrides.
        s3::input::xbox::XboxState state{};
        state.buttons = 0;
        state.triggers = {0, 0};
        state.left = {0, 0};
        state.right = {0, 0};
        input.sample_raw_for_test(state);
        auto actions = input.player_actions();
        const auto updated = controls.update_for_physics(actions, physics, skater, camera);
        CHECK_MSG(updated.is_ok(), err_text(updated));
        const auto advanced = frame::advance(physics, skater, controls, graphs, actions, true, camera);
        CHECK_MSG(advanced.is_ok(), "Map gameplay tick " << physics.ticks << ": " << err_text(advanced));
        assert_publication();
    }

    void assert_publication() const {
        const std::uint64_t tick = physics.ticks;
        CHECK_MSG(!physics.failed, "Physics failed at tick " << tick);
        auto check_body = [&](const auto& body) {
            CHECK_MSG(all_finite(xyz(body.rates.position)), "Non-finite body position at tick " << tick);
            bool finite = true;
            for (const auto& column : body.rates.basis.columns) {
                for (const float v : column) finite = finite && std::isfinite(v);
            }
            CHECK_MSG(finite, "Non-finite body orientation at tick " << tick);
        };
        for (const auto& body : physics.board.bodies()) check_body(body);
        for (const auto& body : skater.skeleton.bodies()) check_body(body);
        CHECK(!skater.render_pose.empty());
        bool pose_finite = true;
        for (const auto& m : skater.render_pose) {
            for (const auto& row : m) {
                for (const float v : row) pose_finite = pose_finite && std::isfinite(v);
            }
        }
        CHECK_MSG(pose_finite, "Non-finite render pose at tick " << tick);
        bool root_finite = true;
        for (const auto& row : skater.animated_skeleton.roots.animation_to_world) {
            for (const float v : row) root_finite = root_finite && std::isfinite(v);
        }
        CHECK_MSG(root_finite, "Non-finite player root at tick " << tick);
        CHECK_EQ(skater.pose_generation, tick);
        CHECK_EQ(skater.animation.ticks, tick);
        CHECK_MSG(camera.frame.has_value(), "Gameplay camera must publish");
        const auto& frame = *camera.frame;
        bool camera_finite = true;
        for (const float v : frame.position) camera_finite = camera_finite && std::isfinite(v);
        for (const auto& column : frame.basis.columns) {
            for (const float v : column) camera_finite = camera_finite && std::isfinite(v);
        }
        CHECK(camera_finite);
        CHECK(std::isfinite(frame.field_of_view_degrees));
        CHECK(frame.field_of_view_degrees > 0.0f && frame.field_of_view_degrees < 180.0f);
        CHECK_MSG(camera.latest_subject.has_value(), "Missing camera subject");
        const auto& subject = *camera.latest_subject;
        CHECK_MSG(subject.tick == tick - 1, "Camera consumed stale physical output");
        std::array<float, 4> published_com{};
        for (std::size_t i = 0; i < 4; ++i) {
            published_com[i] = s3::rs::from_bits(skater.player_input.physical.reckoning.vector_64[i]);
        }
        close(head3(subject.pose.center_of_mass), head3(published_com),
              "Camera must consume the completed physical COM publication");
    }

    void run_supported_startup(const graph_runtime::StockGraphs& graphs, const SkateMap& map) {
        bool saw_solved_wheel = false;
        std::size_t grounded_ticks = 0;
        for (std::uint64_t i = 0; i < TICKS; ++i) {
            advance(graphs);
            grounded_ticks += physics.riding.ground.wheel_contact_count > 0 ? 1 : 0;
            for (const auto& report : physics.board.contact_reports()) {
                const F3 force = xyz(report.normal_force_on_a);
                CHECK(all_finite(force));
                saw_solved_wheel =
                    saw_solved_wheel ||
                    (s3::physics::board::index(report.part) < 4 &&
                     report.other.kind == s3::physics::board_step::CollisionBody::Kind::StaticWorld &&
                     (force[0] != 0.0f || force[1] != 0.0f || force[2] != 0.0f));
            }
        }
        CHECK_EQ(physics.ticks, TICKS);
        CHECK_MSG(grounded_ticks > 0, "Map \"" << map.name << "\": wheel queries never found support");
        CHECK_MSG(physics.riding.ground.wheel_contact_count > 0,
                  "Map \"" << map.name << "\": no wheel support after " << TICKS << " neutral gameplay ticks");
        CHECK_MSG(saw_solved_wheel, "Map \"" << map.name << "\": no solved wheel contact reaction");
        const auto deck = physics.board.part_transforms()[s3::physics::board::index(BodyId::Deck)].translation;
        std::fprintf(stderr,
                     "Map \"%s\": spawn=[%g, %g, %g], yaw=%g, ticks=%llu, grounded_ticks=%zu, deck=(%g, %g, %g)\n",
                     map.name.c_str(), static_cast<double>(map.spawn[0]), static_cast<double>(map.spawn[1]),
                     static_cast<double>(map.spawn[2]), static_cast<double>(map.heading),
                     static_cast<unsigned long long>(physics.ticks), grounded_ticks, static_cast<double>(deck.x),
                     static_cast<double>(deck.y), static_cast<double>(deck.z));
    }
};

}  // namespace

// #[ignore = "requires SKATE3_ASSET_ROOT with private stock skater, graph and camera assets"]
S3_TEST(moved_demo_starts_board_skater_and_camera_at_authored_spawn_and_yaw) {
    const auto* vfs = s3test::game::assets();
    if (vfs == nullptr) return;
    const std::string& root = s3test::game::ASSET_ROOT;
    auto assets = s3::data::GameAssets::load(*vfs, root);
    CHECK_MSG(assets.ok, assets.error);
    auto graphs = graph_runtime::StockGraphs::load(*vfs, root, assets.value).unwrap();
    SkateMap original;
    if (!load_map(s3test::game::map_path(), original)) return;
    SkateMap moved = original;
    moved.spawn = relocate(original.spawn);
    moved.heading += s3::rs::f32::consts::FRAC_PI_2;
    for (auto& vertex : moved.geometry.vertices) {
        vertex.position = relocate(vertex.position);
        vertex.normal = rotate(vertex.normal);
    }
    for (auto& triangle : moved.geometry.collision) {
        for (auto& p : triangle.points) p = relocate(p);
    }
    const auto baseline = Gameplay::load(*vfs, root, graphs, original);
    auto gameplay = Gameplay::load(*vfs, root, graphs, moved);
    CHECK_EQ(gameplay.physics.world.triangles().size(), moved.geometry.collision.size());
    {
        const auto& loaded = gameplay.physics.world.triangles();
        for (std::size_t t = 0; t < loaded.size() && t < moved.geometry.collision.size(); ++t) {
            for (std::size_t k = 0; k < 3; ++k) {
                close(xyz(loaded[t].triangle.vertices[k]), moved.geometry.collision[t].points[k],
                      "Production world must contain moved map collision");
            }
        }
    }
    {
        const auto actual = gameplay.physics.board.part_transforms();
        const auto originals = baseline.physics.board.part_transforms();
        for (std::size_t i = 0; i < actual.size(); ++i) {
            close(xyz(actual[i].translation), relocate(xyz(originals[i].translation)), "Board part spawn");
            for (std::size_t c = 0; c < 3; ++c) {
                close(actual[i].basis.columns[c], rotate(originals[i].basis.columns[c]), "Board part yaw");
            }
        }
    }
    const auto wheel = gameplay.physics.board.part_transforms()[0].translation;
    CHECK_MSG(std::fabs(wheel.y - gameplay.physics.settings.wheel_radius - moved.spawn[1]) < POSE_EPSILON,
              "Map spawn must anchor the authored wheel bottom to the ground");
    CHECK_EQ(gameplay.skater.skeleton.bodies().size(), std::size_t(26));
    {
        const auto actual = gameplay.skater.skeleton.part_transforms();
        const auto originals = baseline.skater.skeleton.part_transforms();
        for (std::size_t i = 0; i < actual.size(); ++i) {
            close(head3(actual[i][3]), relocate(head3(originals[i][3])), "Skater body spawn");
            for (std::size_t axis = 0; axis < 3; ++axis) {
                close(head3(actual[i][axis]), rotate(head3(originals[i][axis])), "Skater body yaw");
            }
        }
    }
    {
        const auto actual = gameplay.skater.animated_skeleton.roots.animation_to_world;
        const auto original_root = baseline.skater.animated_skeleton.roots.animation_to_world;
        close(head3(actual[3]), relocate(head3(original_root[3])), "Presented player root spawn");
        for (std::size_t axis = 0; axis < 3; ++axis) {
            close(head3(actual[axis]), rotate(head3(original_root[axis])), "Presented player root yaw");
        }
    }
    gameplay.run_supported_startup(graphs, moved);
    const auto deck = gameplay.physics.board.part_transforms()[s3::physics::board::index(BodyId::Deck)].translation;
    CHECK_MSG(deck.y > moved.spawn[1], "Deck fell below the moved demo floor");
    const auto& camera = gameplay.camera.frame.value();
    // A broad startup acceptance bound: catches a camera left at the origin.
    // It does not prescribe a chase distance or replace the stock camera tuning.
    float sum = -0.0f;
    for (std::size_t i = 0; i < 3; ++i) {
        const float d = camera.position[i] - moved.spawn[i];
        sum += d * d;
    }
    const float camera_distance = s3::rs::sqrt(sum);
    CHECK_MSG(camera_distance < 20.0f, "Camera did not follow moved spawn: " << camera_distance << "m");
}

// #[ignore = "requires SKATE3_ASSET_ROOT and SKATE_MAP_TEST_PATH pointing to a private extracted .skate map"]
S3_TEST(private_extracted_map_supports_production_gameplay_startup) {
    const auto* vfs = s3test::game::assets();
    if (vfs == nullptr) return;
    const std::string& root = s3test::game::ASSET_ROOT;
    const char* env = std::getenv("SKATE_MAP_TEST_PATH");
    const std::string path = env != nullptr ? std::string(env) : s3test::game::map_path();
    SkateMap map;
    if (!load_map(path, map)) return;
    auto assets = s3::data::GameAssets::load(*vfs, root);
    CHECK_MSG(assets.ok, assets.error);
    auto graphs = graph_runtime::StockGraphs::load(*vfs, root, assets.value).unwrap();
    auto gameplay = Gameplay::load(*vfs, root, graphs, map);
    CHECK(!gameplay.physics.world.triangles().empty());
    const float x = map.spawn[0];
    const float y = map.spawn[1];
    const float z = map.spawn[2];
    const auto support =
        gameplay.physics.world.query_thin_line(Vector3{x, y + 1.0f, z}, Vector3{x, y - 10.0f, z}).unwrap();
    CHECK_MSG(support.has_value(), path << ": authored spawn has no supporting map collision");
    const auto deck = gameplay.physics.board.part_transforms()[s3::physics::board::index(BodyId::Deck)];
    close(xyz(deck.translation),
          {x, y + gameplay.physics.settings.wheel_radius - gameplay.physics.settings.authored[0].translation.y, z},
          "Private map deck spawn");
    const float sin = std::sin(map.heading);
    const float cos = std::cos(map.heading);
    close(deck.basis.columns[2], {sin, 0.0f, cos}, "Private map deck heading");
    std::fprintf(stderr, "Private map %s: %zu collision triangles\n", path.c_str(),
                 gameplay.physics.world.triangles().size());
    gameplay.run_supported_startup(graphs, map);
}
