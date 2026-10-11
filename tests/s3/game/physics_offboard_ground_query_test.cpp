// Port of skate-game/src/physics/offboard/ground_query/tests.rs
#include <cmath>

#include "harness.hpp"
#include "s3/game/physics/offboard/ground_query.hpp"

using namespace s3::game::physics::offboard::ground_query;
using s3::physics::board_world::query_metadata::QueryMesh;
using s3::physics::board_world::query_metadata::QueryMetadata;
using s3::physics::board_world::query_metadata::QueryPool;
using s3::physics::contact::RetailContactMaterial;
using s3::physics::drive_frames::RetailAffineTransform;
using s3::player::offboard::ground_query::Line;
using s3::player::offboard::ground_query::QueryContext;
using WorldBounds = s3::physics::board_world::query_metadata::Bounds;
using Hits = std::array<std::optional<LineHit>, 7>;

S3_TEST(indexed_edges_sort_ids_and_preserve_shared_capacity) {
    const Bounds bounds{Vector3::new_(-1.0f, -1.0f, -1.0f), Vector3::new_(1.0f, 1.0f, 1.0f)};
    const std::array<Segment, 1> segments{Segment{Edge{Vector3::ZERO, Vector3::new_(1.0f, 0.0f, 0.0f)}, bounds}};
    const EdgeBody first{Frame::IDENTITY, bounds, Slice<const Segment>(segments)};
    Frame second_frame = Frame::IDENTITY;
    second_frame.position = Vector3::new_(0.0f, 0.25f, 0.0f);
    const EdgeBody second{second_frame, bounds, Slice<const Segment>(segments)};
    const std::array<IndexedEdgeBody, 2> entries{IndexedEdgeBody{9, false, &first}, IndexedEdgeBody{2, false, &second}};
    const std::vector<Segment> static_edges(39, segments[0]);
    Scene scene({}, {}, {}, 0, Slice<const Segment>(static_edges), PrimaryEdges::Normal{},
                Slice<const IndexedEdgeBody>(entries));
    EdgeSearch search{};
    search.min = bounds.min;
    search.max = bounds.max;
    search.frame = Frame::IDENTITY;
    search.context = QueryContext{0, -1};
    search.narrow_forward = false;
    const auto result = scene.edge_candidates(search).unwrap();
    CHECK_EQ(result.size(), std::size_t(40));
    CHECK_EQ(result[39].start.y, 0.25f);
}

S3_TEST(decoded_face_uses_world_vertices_and_inclusive_bounds) {
    const auto face =
        transform::face({Vector3::ZERO, Vector3::new_(0.0f, 0.0f, 2.0f), Vector3::new_(3.0f, 0.0f, 0.0f)});
    CHECK(std::fabs(face.y - 1.0f) < 1e-6f);
    const Bounds a{Vector3::ZERO, Vector3::new_(1.0f, 1.0f, 1.0f)};
    const Bounds b{Vector3::new_(1.0f, 1.0f, 1.0f), Vector3::new_(2.0f, 2.0f, 2.0f)};
    CHECK(transform::overlaps(a, b));
    CHECK(transform::matches(-1, 3));
    CHECK(!transform::matches(2, 3));
}

namespace {
WorldTriangle triangle(float y) {
    return *WorldTriangle::from_vertices(
        {Vector3::new_(-2.0f, y, -2.0f), Vector3::new_(-2.0f, y, 2.0f), Vector3::new_(2.0f, y, -2.0f)},
        RetailContactMaterial{0.0f, 0.0f, 0.0f}, 123, 0, {0.0f, 0.0f, 0.0f}, 0.0f);
}
GroundQueryPacket packet() {
    GroundQueryPacket p{};
    p.context = QueryContext{0, 7};
    p.center = Vector3::ZERO;
    p.up = Vector3::new_(0.0f, 1.0f, 0.0f);
    p.tangent = Vector3::new_(0.0f, 0.0f, 1.0f);
    p.lines.fill(Line{Vector3::new_(-0.5f, 2.0f, -0.5f), Vector3::new_(-0.5f, -2.0f, -0.5f), 0.0f});
    return p;
}
template <std::size_t N, std::size_t M>
Mesh mesh(const std::array<WorldTriangle, N>& triangles, const std::array<std::uint16_t, M>& surfaces,
          std::int32_t matching_group) {
    Mesh m{};
    m.triangles = Slice<const WorldTriangle>(triangles);
    m.surfaces = Slice<const std::uint16_t>(surfaces);
    m.matching_group = matching_group;
    m.local_to_world = Frame::IDENTITY;
    m.world_to_local = Frame::IDENTITY;
    m.local_bounds = Bounds{Vector3::new_(-2.0f, -2.0f, -2.0f), Vector3::new_(2.0f, 2.0f, 2.0f)};
    return m;
}
s3::rs::Result<Hits, const char*> query_world(const BoardWorld& world) {
    return with_world_scene<Hits>(world, PrimaryEdges::Normal{}, Slice<const IndexedEdgeBody>{},
                                  [](world::WorldScene& scene) { return scene.query_lines(packet()); });
}
}  // namespace

S3_TEST(actual_triangle_queries_preserve_pool_ties_group_filter_and_conditional_gate) {
    const std::array<WorldTriangle, 1> triangles{triangle(0.0f)};
    const std::array<WorldTriangle, 1> upper{triangle(1.0f)};
    const std::array<std::uint16_t, 1> s11{11}, s22{22}, s33{33};
    const std::array<Mesh, 1> ground{mesh(triangles, s11, 7)};
    const std::array<Mesh, 1> island{mesh(triangles, s22, -1)};
    const std::array<Mesh, 1> conditional{mesh(upper, s33, 7)};
    Scene scene(Slice<const Mesh>(ground), Slice<const Mesh>(island), Slice<const Mesh>(conditional), 0, {},
                PrimaryEdges::Normal{}, {});
    const auto p = packet();
    const auto hit = *scene.query_lines(p).unwrap()[0];
    CHECK_EQ(hit.packed_surface, std::uint16_t(11));
    CHECK_EQ(hit.fraction, 0.5f);
    CHECK(hit.face_normal.y > 0.99f);
    auto different = p;
    different.context.matching_id_2952 = 8;
    CHECK_EQ(scene.query_lines(different).unwrap()[0]->packed_surface, std::uint16_t(22));
    scene.island_flags = 3;
    CHECK_EQ(scene.query_lines(p).unwrap()[0]->packed_surface, std::uint16_t(33));
}

S3_TEST(canonical_world_bridge_requires_metadata_and_queries_authored_surfaces) {
    const auto unannotated = BoardWorld::new_({triangle(0.0f)});
    CHECK(query_world(unannotated).is_err());
    QueryMetadata metadata{};
    metadata.packed_surfaces = {0x400};
    metadata.island_flags = 0;
    QueryMesh m{};
    m.triangle_range = {0, 1};
    m.local_to_world = RetailAffineTransform::IDENTITY;
    m.world_to_local = RetailAffineTransform::IDENTITY;
    m.local_bounds = WorldBounds{Vector3::new_(-2.0f, 0.0f, -2.0f), Vector3::new_(2.0f, 0.0f, 2.0f)};
    m.matching_group = 7;
    m.pool = QueryPool::Ground;
    m.rejection_flags = 0;
    m.geometry = 73;
    metadata.meshes = {m};
    const auto world = BoardWorld::with_query_metadata({triangle(0.0f)}, metadata).unwrap();
    const auto hits = query_world(world).unwrap();
    CHECK_EQ(hits[0]->packed_surface, std::uint16_t(0x400));
}

S3_TEST(indexed_world_bridge_retains_group_pool_and_conditional_selection) {
    std::vector<WorldTriangle> triangles;
    std::vector<QueryMesh> meshes;
    for (std::size_t i = 0; i < 256; ++i) {
        const auto original = triangle(i == 102 ? 1.0f : 0.0f);
        const float shift = (i >= 100 && i <= 102) ? 0.0f : 1000.0f + static_cast<float>(i) * 10.0f;
        std::array<Vector3, 3> vertices{};
        for (std::size_t k = 0; k < 3; ++k) {
            const auto v = original.triangle.vertices[k];
            vertices[k] = Vector3::new_(v.x + shift, v.y, v.z);
        }
        triangles.push_back(
            *WorldTriangle::from_vertices(vertices, original.material, 123, 0, {0.0f, 0.0f, 0.0f}, 0.0f));
        QueryMesh m{};
        m.triangle_range = {i, i + 1};
        m.local_to_world = RetailAffineTransform::IDENTITY;
        m.world_to_local = RetailAffineTransform::IDENTITY;
        m.local_bounds = *WorldBounds::from_points(vertices);
        m.matching_group = i == 100 ? 8 : -1;
        m.rejection_flags = 0;
        m.geometry = static_cast<std::uint32_t>(i);
        m.pool = i == 101 ? QueryPool::Island : (i == 102 ? QueryPool::Conditional : QueryPool::Ground);
        meshes.push_back(m);
    }
    QueryMetadata metadata{};
    for (std::uint16_t i = 0; i < 256; ++i) metadata.packed_surfaces.push_back(i);
    metadata.meshes = meshes;
    metadata.island_flags = 0;
    for (const auto& [flags, expected] : {std::pair<std::uint32_t, std::uint16_t>{0, 101}, {3, 102}}) {
        metadata.island_flags = flags;
        const auto world = BoardWorld::with_query_metadata(triangles, metadata).unwrap();
        const auto hits = query_world(world).unwrap();
        for (const auto& hit : hits) {
            CHECK(hit.has_value());
            CHECK_EQ(hit->packed_surface, expected);
        }
    }
}
