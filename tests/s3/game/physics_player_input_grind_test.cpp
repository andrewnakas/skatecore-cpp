// Port of skate-game/src/physics/player_input/grind/tests.rs (mounted as
// physics::player_input::grind::tests) and grind/world_tests.rs (mounted as
// physics::player_input::grind::world::tests).
#include <cmath>

#include "harness.hpp"
#include "s3/core/physics/board_world/query_metadata.hpp"
#include "s3/core/physics/triangle_query.hpp"
#include "s3/game/physics/player_input/grind.hpp"
#include "s3/game/physics/player_input/grind/history.hpp"
#include "s3/game/physics/player_input/grind/publication.hpp"
#include "s3/game/physics/player_input/grind/world.hpp"

using namespace s3::game::physics::player_input::grind;
using s3::point_graph::PointGraph;

// ---- grind/tests.rs ----

namespace {

GrindInputState state() {
    auto constant = [](float value) {
        PointGraph<4> g;
        g.x = {0.0f, 1.0f, 2.0f, 3.0f};
        g.y = {value, value, value, value};
        return g;
    };
    GrindInputState s;
    s.previous_state = 0;
    s.engagement_counter = 0;
    s.cooldown = 0;
    s.disabled = false;
    s.suppressed = false;
    s.elapsed = 0.0f;
    s.friction_vs_time = 0.0f;
    s.previous_velocity = {0, 0, 0, 0};
    s.grind_history = 0;
    s.secondary_history = 0;
    s.grounded_frames = 0;
    s.air_frames = 0;
    s.low_wheel_frames = 0;
    s.previous_proximity = false;
    s.previous_air_target = false;
    s.previous_direction = {0.0f, 0.0f, 0.0f, 0.0f};
    s.gravity_timer = 0.0f;
    s.balance = balance::BalanceState::default_();
    s.engagement = entry::Engagement{};
    s.control = control::Control{};
    s.jumper = manager::Jumper::default_();
    s.investigation = GrindInvestigationFields{};
    s.settings.friction = constant(0.8f);
    s.settings.slope_threshold = constant(1.0f);
    s.settings.vertical_help = constant(0.5f);
    s.settings.gravity_vertical = constant(2.0f);
    s.settings.gravity_linear = constant(0.5f);
    s.settings.exit_lean.x = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f};
    s.settings.exit_lean.y = {};
    s.settings.truck_to_wheel = 0.095f;
    s.settings.deck_to_truck = 0.243f;
    s.settings.test_above = 0.03f;
    s.settings.test_below = 0.2f;
    s.settings.max_impact = 8.0f;
    return s;
}

PreContext context() {
    PreContext c;
    c.board = {V{1.0f, 0.0f, 0.0f, 0.0f}, V{0.0f, 1.0f, 0.0f, 0.0f}, V{0.0f, 0.0f, 1.0f, 0.0f},
               V{0.0f, 0.0f, 0.0f, 1.0f}};
    c.air_counter = 20;
    c.tip_state = 0;
    c.air_targeting_grind_9653 = false;
    c.balance_2720 = 0.0f;
    c.translation_2796 = 0.0f;
    c.stability_nudge_2800 = 0.0f;
    c.up_down_2804 = 0.0f;
    c.grab_min_height_2808 = 0.0f;
    return c;
}

struct TestHost final : Host {
    std::vector<std::string> events;
    bool fail = false;

    s3::game::Status apply_material_mode(MaterialMode mode) override {
        events.push_back(mode == MaterialMode::Grind      ? "grind"
                         : mode == MaterialMode::Standard ? "standard"
                                                          : "unchanged");
        if (fail) return s3::rs::make_err(std::string("material failure"));
        return s3::game::ok();
    }
    s3::rs::Result<std::optional<ProbeHit>, std::string> surface_probe(const BoardWorld&, std::array<std::uint32_t, 2>,
                                                                       std::size_t, Probe) override {
        events.push_back("surface");
        if (fail) return s3::rs::make_err(std::string("query failure"));
        return std::optional<ProbeHit>{};
    }
    s3::rs::Result<std::optional<balance::ForceExitHit>, std::string> force_exit_line(
        const BoardWorld&, std::array<std::uint32_t, 2>, balance::ForceExitProbe) override {
        events.push_back("exit");
        if (fail) return s3::rs::make_err(std::string("query failure"));
        return std::optional<balance::ForceExitHit>{};
    }
};

}  // namespace

S3_TEST(material_selection_uses_previous_air_target_and_preserves_skipped_history) {
    auto s = state();
    ProcessedPhysicsInput p{};
    p.state_2508 = 201;
    p.category_2512 = 200;
    CHECK(s.material_mode(p, false, true) == MaterialMode::Standard);
    p.state_2508 = 300;
    CHECK(s.material_mode(p, true, false) == MaterialMode::Unchanged);
    CHECK(s.previous_air_target);
    p.state_2508 = 100;
    p.category_2512 = 100;
    CHECK(s.material_mode(p, false, false) == MaterialMode::Grind);
    CHECK(s.material_mode(p, false, false) == MaterialMode::Standard);
}

S3_TEST(permission_keeps_flicker_weights_strict_threshold_and_signed_decrement) {
    auto s = state();
    ProcessedPhysicsInput p{};
    p.state_2508 = 400;
    s.permission(p, 20);
    CHECK_EQ(s.engagement_counter, 49u);
    p.state_2508 = 403;
    s.permission(p, 20);
    CHECK_EQ(s.engagement_counter, 68u);
    s.engagement_counter = 152;
    s.permission(p, 20);
    CHECK(!s.suppressed);
    s.engagement_counter = 153;
    s.permission(p, 20);
    CHECK(s.suppressed && s.disabled);
    CHECK_EQ(s.cooldown, 90u);
    CHECK_EQ(history::decrement(0), 0u);
    CHECK_EQ(history::decrement(UINT32_MAX), 0u);
    CHECK_EQ(history::decrement(0x80000000u), 0x7fffffffu);
}

S3_TEST(constructor_reset_does_not_reinitialize_child_history_or_jump_energy) {
    auto s = state();
    s.jumper.energy = 0.6f;
    s.balance.elapsed = 1.0f;
    s.secondary_history = 15;
    s.investigation.flags_1516 = UINT32_MAX;
    s.reset();
    CHECK(s.investigation == GrindInvestigationFields{});
    CHECK_EQ(s.jumper.energy, 0.6f);
    CHECK_EQ(s.balance.elapsed, 1.0f);
    CHECK_EQ(s.secondary_history, 15u);
}

S3_TEST(empty_authored_world_completes_active_grind_without_error_stub) {
    auto s = state();
    ProcessedPhysicsInput p{};
    p.state_2508 = 403;
    p.category_2512 = 400;
    p.timestep_2604 = 0.02f;
    p.grind_words_2532_2536 = {3, 0};
    const auto provider = StaticProvider::new_(nullptr).unwrap();
    const auto world = BoardWorld::new_({});
    TestHost host;
    auto pending = s.pre_update(p, provider, world, context(), host).unwrap();
    const auto c = context();
    PostContext post;
    post.board = c.board;
    post.balance_2720 = c.balance_2720;
    post.translation_2796 = c.translation_2796;
    post.stability_nudge_2800 = c.stability_nudge_2800;
    post.up_down_2804 = c.up_down_2804;
    post.grab_min_height_2808 = c.grab_min_height_2808;
    const auto result = s.post_update(p, world, std::move(pending), post, host).unwrap();
    CHECK(!p.grind.valid_1488);
    CHECK_EQ(p.grind.friction_1496, 0.8f);
    CHECK((p.grind.normal_1152 == std::array<std::uint32_t, 4>{0, 0, 0, 0}));
    CHECK((host.events == std::vector<std::string>{"grind"}));
    CHECK(result.wipeout_reasons.empty());
    CHECK_EQ(static_cast<std::uint32_t>(result.observation.jumper.family_20), 3u);
}

S3_TEST(producer_failure_is_not_a_query_miss) {
    auto s = state();
    const ProcessedPhysicsInput p{};
    const auto provider = StaticProvider::new_(nullptr).unwrap();
    const auto world = BoardWorld::new_({});
    TestHost host;
    host.fail = true;
    CHECK(s.pre_update(p, provider, world, context(), host).is_err());
    CHECK_EQ(s.grounded_frames, 0u);
}

S3_TEST(full_surface_publication_keeps_separate_normals_materials_and_offsets) {
    GrindSurface surface;
    surface.center = {1.0f, 1.0f, 1.0f, 1.0f};
    surface.far_points = {V{2.0f, 2.0f, 2.0f, 2.0f}, V{3.0f, 3.0f, 3.0f, 3.0f}};
    surface.upmost_normal = {4.0f, 4.0f, 4.0f, 4.0f};
    surface.direction = {5.0f, 5.0f, 5.0f, 5.0f};
    surface.high_side = {6.0f, 6.0f, 6.0f, 6.0f};
    surface.normal_limits = {7.0f, 8.0f};
    surface.kind = grind_surface::GeometryType::Ledge;
    surface.audio_surface = 9;
    surface.physics_surface = 10;
    surface.flags = 0x10000000u;
    surface.tilted_upmost_normal = {11.0f, 11.0f, 11.0f, 11.0f};
    GrindInvestigationFields fields{};
    publication::surface(fields, surface);
    CHECK(fields.center_1360 == raw({1.0f, 1.0f, 1.0f, 1.0f}));
    CHECK((fields.far_points_1376_1392 == std::array<std::array<std::uint32_t, 4>, 2>{
                                              raw({2.0f, 2.0f, 2.0f, 2.0f}), raw({3.0f, 3.0f, 3.0f, 3.0f})}));
    CHECK(fields.upmost_normal_1408 == raw({4.0f, 4.0f, 4.0f, 4.0f}));
    CHECK_EQ(fields.audio_surface_1468, 9u);
    CHECK_EQ(fields.physics_surface_1472, 10u);
    CHECK((fields.normal_1152 == std::array<std::uint32_t, 4>{0, 0, 0, 0}));
}

// ---- grind/world_tests.rs ----

namespace {

using namespace s3::game::physics::player_input::grind::world;
using s3::math::Vector3;
using s3::physics::board_world::WorldTriangle;
using s3::physics::board_world::query_metadata::Bounds;
using s3::physics::board_world::query_metadata::QueryMesh;
using s3::physics::board_world::query_metadata::QueryMetadata;
using s3::physics::board_world::query_metadata::QueryPool;
using s3::physics::contact::RetailContactMaterial;
using s3::physics::drive_frames::RetailAffineTransform;
using s3::physics::triangle_query::triangle_segment;
using s3::physics::triangle_query::TriangleLineHit;

WorldTriangle triangle(float height, float fatness) {
    return *WorldTriangle::from_vertices(
        {Vector3::new_(-2.0f, height, -2.0f), Vector3::new_(-2.0f, height, 2.0f), Vector3::new_(2.0f, height, -2.0f)},
        RetailContactMaterial{0.7f, 0.4f, 0.2f}, 0xdeadbeefu, 0xe0, {1.0f, 1.0f, 1.0f}, fatness);
}

struct Entry {
    float height;
    QueryPool pool;
    std::int32_t group;
    std::uint16_t surface;
};

BoardWorld make_world(const std::vector<Entry>& entries, std::uint32_t flags, float fatness) {
    std::vector<WorldTriangle> triangles;
    for (const auto& e : entries) triangles.push_back(triangle(e.height, fatness));
    QueryMetadata metadata;
    for (std::size_t index = 0; index < entries.size(); ++index) {
        QueryMesh mesh;
        mesh.triangle_range = {index, index + 1};
        mesh.local_to_world = RetailAffineTransform::IDENTITY;
        mesh.world_to_local = RetailAffineTransform::IDENTITY;
        mesh.local_bounds = *Bounds::from_points(triangles[index].triangle.vertices);
        mesh.matching_group = entries[index].group;
        mesh.rejection_flags = UINT32_MAX;
        mesh.geometry = 700;
        mesh.pool = entries[index].pool;
        metadata.meshes.push_back(mesh);
        metadata.packed_surfaces.push_back(entries[index].surface);
    }
    metadata.island_flags = flags;
    return BoardWorld::with_query_metadata(std::move(triangles), std::move(metadata)).unwrap();
}

Probe down() { return Probe{V{-0.5f, 1.0f, -0.5f, 0.0f}, V{-0.5f, -1.0f, -0.5f, 0.0f}, 0.0f}; }

const std::array<std::uint32_t, 2> ZERO2 = {0, 0};

}  // namespace

S3_TEST(exact_packed_surface_not_render_tag_and_zero_rejection_mask) {
    const auto w = make_world({{0.0f, QueryPool::Ground, 7, 0xf3a9}}, 0, 0.0f);
    const auto hit = *surface_probe(w, {700, 7}, 0, down()).unwrap();
    CHECK_EQ(hit.packed_surface, 0xf3a9u);
    CHECK_EQ(hit.fraction, 0.5f);
    CHECK((hit.normal == V{0.0f, 1.0f, 0.0f, 0.0f}));
    // Actor2948 equals mesh geometry, but neither participates in this filter.
    CHECK(!surface_probe(w, {7, 8}, 0, down()).unwrap().has_value());
    CHECK(surface_probe(w, {0, UINT32_MAX}, 0, down()).unwrap().has_value());
    const auto wildcard = make_world({{0.0f, QueryPool::Ground, -1, 5}}, 0, 0.0f);
    CHECK(surface_probe(wildcard, {0, 999}, 0, down()).unwrap().has_value());
}

S3_TEST(pool_order_precedes_mesh_order_and_nearest_still_wins) {
    auto w = make_world({{0.0f, QueryPool::Island, -1, 1}, {0.0f, QueryPool::Ground, -1, 2}, {0.0f, QueryPool::Ground, -1, 3}},
                   0, 0.0f);
    CHECK_EQ(surface_probe(w, ZERO2, 0, down()).unwrap()->packed_surface, 2u);
    w = make_world({{0.0f, QueryPool::Ground, -1, 2}, {0.5f, QueryPool::Island, -1, 1}}, 0, 0.0f);
    CHECK_EQ(surface_probe(w, ZERO2, 0, down()).unwrap()->packed_surface, 1u);
}

S3_TEST(conditional_pool_requires_exact_island_state_three) {
    for (std::uint32_t flags : {0u, 1u, 2u, 3u, 4u, 7u}) {
        const auto w = make_world({{0.0f, QueryPool::Conditional, -1, 1}}, flags, 0.0f);
        CHECK_EQ(surface_probe(w, ZERO2, 0, down()).unwrap().has_value(), flags == 3);
    }
}

S3_TEST(contact_fatness_does_not_move_the_query_hit) {
    const auto w = make_world({{0.0f, QueryPool::Ground, -1, 1}}, 0, 0.5f);
    const auto hit = *surface_probe(w, ZERO2, 0, down()).unwrap();
    CHECK_EQ(hit.fraction, 0.5f);
    CHECK_EQ(hit.position[1], 0.0f);
}

S3_TEST(rounded_edge_hit_publishes_authored_face_not_sweep_normal) {
    const auto w = make_world({{0.0f, QueryPool::Ground, -1, 1}}, 0, 0.0f);
    const Probe probe{V{-2.0005f, 0.01f, -1.0f, 0.0f}, V{-2.0005f, -0.01f, -1.0f, 0.0f}, 0.001f};
    TriangleLineHit leaf{Vector3::ZERO, Vector3::ZERO, 0.0f, {0.0f, 0.0f, 0.0f}};
    CHECK(triangle_segment(leaf, vector(probe.start), sub(vector(probe.end), vector(probe.start)),
                           w.triangles()[0].triangle.vertices, probe.radius, 0.0f));
    CHECK(std::fabs(leaf.normal.x) > 0.1f);
    const auto hit = *surface_probe(w, ZERO2, 5, probe).unwrap();
    CHECK((hit.normal == V{0.0f, 1.0f, 0.0f, 0.0f}));
    CHECK(hit.position == packed(leaf.position));
}

S3_TEST(exact_outer_bounds_reject_leaf_endpoint_tolerance) {
    const auto w = make_world({{0.0f, QueryPool::Ground, -1, 1}}, 0, 0.0f);
    Probe probe = down();
    probe.end = {-0.5f, 0.000001f, -0.5f, 0.0f};
    TriangleLineHit leaf{Vector3::ZERO, Vector3::ZERO, 0.0f, {0.0f, 0.0f, 0.0f}};
    CHECK(triangle_segment(leaf, vector(probe.start), sub(vector(probe.end), vector(probe.start)),
                           w.triangles()[0].triangle.vertices, 0.0f, 0.0f));
    CHECK(!surface_probe(w, ZERO2, 0, probe).unwrap().has_value());
}

S3_TEST(native_short_line_gate_and_force_exit_share_matching_contract) {
    const auto w = make_world({{0.0f, QueryPool::Ground, 9, 1}}, 0, 0.0f);
    const float half = s3::rs::from_bits(0x37800000u) * 0.5f;
    const Probe probe{V{-0.5f, half, -0.5f, 0.0f}, V{-0.5f, -half, -0.5f, 0.0f}, 0.001f};
    CHECK(!surface_probe(w, {0, 9}, 5, probe).unwrap().has_value());
    const auto p = down();
    const balance::ForceExitProbe line{p.start, p.end};
    CHECK(!force_exit_line(w, {9, 8}, line).unwrap().has_value());
    CHECK((force_exit_line(w, {0, 9}, line).unwrap()->normal == V{0.0f, 1.0f, 0.0f, 0.0f}));
}

S3_TEST(triangle_bounds_gate_is_required_even_inside_a_larger_mesh) {
    std::vector<WorldTriangle> triangles = {triangle(0.0f, 0.0f), triangle(2.0f, 0.0f)};
    std::vector<Vector3> points;
    for (const auto& t : triangles)
        for (const auto& v : t.triangle.vertices) points.push_back(v);
    const auto local_bounds = *Bounds::from_points(points);
    QueryMetadata metadata;
    metadata.packed_surfaces = {1, 2};
    QueryMesh mesh;
    mesh.triangle_range = {0, 2};
    mesh.local_to_world = RetailAffineTransform::IDENTITY;
    mesh.world_to_local = RetailAffineTransform::IDENTITY;
    mesh.local_bounds = local_bounds;
    mesh.matching_group = -1;
    mesh.rejection_flags = 0;
    mesh.geometry = 0;
    mesh.pool = QueryPool::Ground;
    metadata.meshes = {mesh};
    metadata.island_flags = 0;
    const auto w = BoardWorld::with_query_metadata(std::move(triangles), std::move(metadata)).unwrap();
    Probe probe = down();
    probe.end = {-0.5f, 0.000001f, -0.5f, 0.0f};
    CHECK(!surface_probe(w, ZERO2, 0, probe).unwrap().has_value());
}

S3_TEST(both_side_collector_does_not_replace_one_sided_thin_leaf) {
    const auto w = make_world({{0.0f, QueryPool::Ground, -1, 1}}, 0, 0.0f);
    const auto d = down();
    CHECK(!surface_probe(w, ZERO2, 0, Probe{d.end, d.start, 0.0f}).unwrap().has_value());
}

S3_TEST(missing_metadata_and_invalid_descriptors_are_errors_not_misses) {
    CHECK(surface_probe(BoardWorld::new_({}), ZERO2, 0, down()).is_err());
    const auto w = make_world({}, 0, 0.0f);
    CHECK(surface_probe(w, ZERO2, 7, down()).is_err());
    Probe negative = down();
    negative.radius = -0.1f;
    CHECK(surface_probe(w, ZERO2, 0, negative).is_err());
    Probe nan = down();
    const float n = std::numeric_limits<float>::quiet_NaN();
    nan.start = {n, n, n, n};
    CHECK(surface_probe(w, ZERO2, 0, nan).is_err());
    CHECK(!surface_probe(w, ZERO2, 0, down()).unwrap().has_value());
}

S3_TEST(outer_fsel_clamps_fraction_without_reconstructing_position) {
    CHECK_EQ(clamp_fraction(-0.000001f), 0.0f);
    CHECK_EQ(clamp_fraction(1.000001f), 1.0f);
    CHECK_EQ(clamp_fraction(std::numeric_limits<float>::quiet_NaN()), 1.0f);
    CHECK_EQ(clamp_fraction(0.25f), 0.25f);
}
