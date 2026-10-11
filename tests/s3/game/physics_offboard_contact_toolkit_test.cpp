// Port of skate-game/src/physics/offboard/contact_toolkit/tests.rs
#include <cmath>

#include "harness.hpp"
#include "s3/game/physics/offboard/contact_toolkit.hpp"

using namespace s3::game::physics::offboard::contact_toolkit;
using s3::math::Vector3;
using s3::physics::board_world::BoardWorld;
using s3::physics::board_world::WorldTriangle;
using s3::physics::board_world::query_metadata::Bounds;
using s3::physics::board_world::query_metadata::QueryMesh;
using s3::physics::board_world::query_metadata::QueryMetadata;
using s3::physics::board_world::query_metadata::QueryPool;
using s3::physics::contact::RetailContactMaterial;
using s3::physics::drive_frames::RetailAffineTransform;
using s3::player::offboard::contact_toolkit::Input;
using s3::player::offboard::contact_toolkit::ProbeLayout;

namespace {
BoardWorld plane(std::uint32_t rejection_flags) {
    const std::array<Vector3, 3> vertices{Vector3::new_(-10.0f, 0.0f, -10.0f), Vector3::new_(-10.0f, 0.0f, 10.0f),
                                          Vector3::new_(10.0f, 0.0f, 0.0f)};
    const RetailContactMaterial material{0.0f, 0.0f, 0.0f};
    const auto triangle = *WorldTriangle::from_vertices(vertices, material, 999, 0, {0.0f, 0.0f, 0.0f}, 0.0f);
    QueryMetadata metadata{};
    metadata.packed_surfaces = {17};
    QueryMesh mesh{};
    mesh.triangle_range = {0, 1};
    mesh.local_to_world = RetailAffineTransform::IDENTITY;
    mesh.world_to_local = RetailAffineTransform::IDENTITY;
    mesh.local_bounds = *Bounds::from_points(vertices);
    mesh.matching_group = -1;
    mesh.rejection_flags = rejection_flags;
    mesh.geometry = 73;
    mesh.pool = QueryPool::Ground;
    metadata.meshes = {mesh};
    metadata.island_flags = 0;
    return BoardWorld::with_query_metadata({triangle}, metadata).unwrap();
}
Input input() {
    return Input::from_vectors({{
        {0.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 0.0f},
    }});
}
}  // namespace

S3_TEST(actual_world_hit_preserves_mesh_identity_and_packed_surface) {
    const auto world = plane(0);
    const auto scene = StaticScene::new_(world).unwrap();
    const auto result = scene.execute(ProbeLayout::stock().prepare(input(), -1)).unwrap();
    CHECK(result.trajectories[0].valid());
    CHECK_EQ(result.trajectories[0].geometry, 73u);
    CHECK_EQ(result.trajectories[0].surface, 17u);
    CHECK(std::fabs(result.trajectories[0].landing_normal[1] - 1.0f) < 1e-5f);
    CHECK_EQ(result.lines.size(), std::size_t(44));
}

S3_TEST(nonfinite_unused_lane_preserves_sweep_hits_but_invalid_xyz_still_fails) {
    const auto world = plane(0);
    const auto scene = StaticScene::new_(world).unwrap();
    auto batch = ProbeLayout::stock().prepare(input(), -1);
    const auto expected = scene.execute(batch).unwrap();
    // Reported crash: finite spatial endpoints, start.w=inf, end.w=NaN.
    for (auto& request : batch.trajectories) {
        request.trajectory.position[3] = s3::rs::f32::INFINITY_;
        request.trajectory.velocity[3] = s3::rs::f32::NEG_INFINITY;
    }
    for (auto& line : batch.lines) {
        line.start[3] = s3::rs::f32::INFINITY_;
        line.end[3] = s3::rs::f32::NAN_;
    }
    const auto actual = scene.execute(batch).unwrap();
    CHECK(actual.trajectories == expected.trajectories);
    CHECK(actual.lines == expected.lines);
    CHECK(actual.edges == expected.edges);
    for (std::size_t axis = 0; axis < 3; ++axis) {
        auto invalid = batch;
        invalid.lines[0].start[axis] = s3::rs::f32::NAN_;
        CHECK(scene.execute(invalid).is_err());
        invalid.lines[0].start[axis] = 0.0f;
        invalid.lines[0].end[axis] = s3::rs::f32::INFINITY_;
        CHECK(scene.execute(invalid).is_err());
    }
    for (float radius : {-1.0f, s3::rs::f32::NAN_, s3::rs::f32::INFINITY_}) {
        auto invalid = batch;
        invalid.lines[0].radius = radius;
        CHECK(scene.execute(invalid).is_err());
    }
}

S3_TEST(mesh_filter_is_not_a_board_flag_or_material_tag) {
    const auto world = plane(0x2000);
    const auto scene = StaticScene::new_(world).unwrap();
    const auto result = scene.execute(ProbeLayout::stock().prepare(input(), -1)).unwrap();
    CHECK(!result.trajectories[0].valid());
    for (const auto& line : result.lines) CHECK(!line.has_value());
}

S3_TEST(canonical_world_reaches_completed_analyzer_without_substitute_support) {
    const auto world = plane(0);
    const auto scene = StaticScene::new_(world).unwrap();
    Owner owner{};
    CHECK(!owner.refresh().has_value());
    CHECK(owner.submit(input(), -1, scene).is_ok());
    const auto completed = *owner.refresh();
    CHECK_EQ(completed.prefix.flags_176 & 1u, 1u);
    CHECK_EQ(completed.prefix.support_180, 73u);
    CHECK(std::fabs(completed.prefix.position[1]) < 1e-5f);
    CHECK(completed.prefix.normal[1] > 0.99f);
    CHECK(completed.samples.original_ground_count > 0);
    CHECK(!owner.has_pending());

    // Move the query outside the authored collision mesh. Previous support
    // cannot be retained as a fabricated surface after the real query misses.
    auto airborne = input();
    airborne.position[1] = 10.0f;
    CHECK(owner.submit(airborne, -1, scene).is_ok());
    const auto missed = *owner.refresh();
    CHECK_EQ(missed.prefix.flags_176 & 1u, 0u);
    CHECK_EQ(missed.prefix.support_180, 0u);
    for (const auto& hit : missed.results.trajectories) CHECK(!hit.valid());
}

S3_TEST(toolkit_refuses_world_without_authored_query_metadata) {
    const auto unannotated = BoardWorld::new_({});
    CHECK(StaticScene::new_(unannotated).is_err());
}

// embedded_static_rwcm_hits_distinct_actor_query_ids: NOT PORTED. It include_bytes! the repo
// fixtures `maps/format-demo.skate` and `skate-data/tests/fixtures/retail-collision.rwcmset`,
// which are not in the reference checkout (see tests/s3/data/skate_map_test.cpp).

S3_TEST(indexed_clusters_preserve_rejection_identity_surface_and_pool_ties) {
    const auto source = plane(0);
    std::vector<WorldTriangle> triangles;
    QueryMetadata metadata = *source.query_metadata().unwrap();
    metadata.meshes.clear();
    metadata.packed_surfaces.clear();
    for (std::size_t i = 0; i < 256; ++i) {
        const float shift = (i >= 100 && i <= 102) ? 0.0f : 1000.0f + static_cast<float>(i) * 30.0f;
        const auto original = source.triangles()[0];
        std::array<Vector3, 3> vertices{};
        for (std::size_t k = 0; k < 3; ++k) {
            const auto v = original.triangle.vertices[k];
            vertices[k] = Vector3::new_(v.x + shift, v.y, v.z);
        }
        triangles.push_back(
            *WorldTriangle::from_vertices(vertices, original.material, 999, 0, {0.0f, 0.0f, 0.0f}, 0.0f));
        auto mesh = source.query_metadata().unwrap()->meshes[0];
        mesh.triangle_range = {i, i + 1};
        mesh.local_bounds = *Bounds::from_points(vertices);
        mesh.geometry = 7000 + static_cast<std::uint32_t>(i);
        mesh.rejection_flags = i == 100 ? 0x2000 : 0;
        mesh.pool = i == 102 ? QueryPool::Island : QueryPool::Ground;
        metadata.meshes.push_back(mesh);
        metadata.packed_surfaces.push_back(static_cast<std::uint16_t>(i));
    }
    const auto world = BoardWorld::with_query_metadata(triangles, metadata).unwrap();
    const auto scene = StaticScene::new_(world).unwrap();
    const auto result = scene.execute(ProbeLayout::stock().prepare(input(), -1)).unwrap();
    CHECK(result.trajectories[0].valid());
    CHECK_EQ(result.trajectories[0].geometry, 7101u);
    CHECK_EQ(result.trajectories[0].surface, 101u);
    for (const auto& hit : result.lines) {
        if (!hit) continue;
        CHECK_EQ(hit->geometry, 7101u);
        CHECK_EQ(hit->surface, std::uint16_t(101));
    }
}
