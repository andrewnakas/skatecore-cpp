// Port of skate-core/src/physics/tests/board_world.rs and
// skate-core/src/physics/board_world/tests.rs (broadphase tests).
#include <sstream>
#include <vector>

#include "harness.hpp"
#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/physics/drive_parameters.hpp"

namespace {
using namespace s3::physics::board_world;
using s3::math::Basis3;
using s3::physics::board::BODY_COUNT;
using s3::physics::board::BodyId_ORDER;
using s3::physics::board::RETAIL_TRUCK_Y_POSITION;
using s3::physics::board::RETAIL_WHEEL_RADIUS;
using s3::physics::board_runtime::BoardMotion;
using s3::physics::board_runtime::BoardRuntime;
using s3::physics::board_step::AttachedStep;
using s3::physics::board_step::BoardStepSettings;
using s3::physics::collision::TriangleFeature;
using s3::physics::drive_frames::authored_body_transforms;
using s3::physics::drive_frames::AuthoredTransformInputs;
using s3::physics::drive_frames::default_truck_transforms;
using s3::physics::drive_frames::RetailAffineTransform;
using s3::physics::drive_parameters::retail_truck_drive_dynamics;
using s3::physics::drive_parameters::RetailTruckDriveSettings;
using s3::physics::rigid_body::RetailBodyMassProperties;
using s3::physics::rigid_body::RetailInertiaDynamics;
using s3::physics::rigid_body::RetailLocalMassFrame;
using s3::physics::rigid_body::RetailSimulationStep;
using s3::physics::world_contact::triangle_from_volume;
using query_metadata::QueryMesh;
using query_metadata::QueryPool;
namespace f32 = s3::rs::f32;

// ---------------- tests/board_world.rs ----------------

RetailContactMaterial material(float a, float b, float c) { return RetailContactMaterial{a, b, c}; }

WorldTriangle triangle() {
    return s3::rs::unwrap(WorldTriangle::from_vertices(
        {Vector3{-10.0f, 0.0f, -10.0f}, Vector3{0.0f, 0.0f, 10.0f}, Vector3{10.0f, 0.0f, -10.0f}},
        material(0.8f, 0.6f, 0.2f), 42, TriangleFeature::ONE_SIDED | 0xe0u, {1.0f, 1.0f, 1.0f}, 0.0f));
}

WheelWorldSettings config() {
    WheelWorldSettings s;
    s.radius = RETAIL_WHEEL_RADIUS;
    s.material = material(0.2f, 0.1f, 0.8f);
    s.query = WorldContactSettings{0.0f, 0.0f, -1.0f, 0.0f, false};
    s.retention = ContactRetentionSettings{100, 0.000001f, false};
    return s;
}

BoardRuntime board(float gap, Vector3 gravity) {
    const RetailBodyMassProperties mass{
        RetailLocalMassFrame::IDENTITY,
        RetailInertiaDynamics{Vector3{3.0f, 3.0f, 3.0f}, 0.5f, 0.0f, 1000.0f, 1000.0f, 0.0f, 0.0f},
    };
    std::array<RetailBodyMassProperties, BODY_COUNT> masses{};
    masses.fill(mass);
    RetailAffineTransform spawn = RetailAffineTransform::IDENTITY;
    spawn.translation = Vector3{0.0f, RETAIL_WHEEL_RADIUS - RETAIL_TRUCK_Y_POSITION + gap, 0.0f};
    return BoardRuntime::new_(masses, authored_body_transforms(AuthoredTransformInputs::STOCK), spawn,
                              RetailSimulationStep::fixed_60_hz(30, 0.001f, gravity), BoardMotion::Active);
}

void close(float a, float b) { CHECK_MSG(s3::rs::abs(a - b) < 2e-6f, a << " != " << b); }

S3_TEST(finite_world_geometry_produces_four_ordered_wheel_contacts_with_combined_materials) {
    BoardWorld world = BoardWorld::new_({triangle()});
    const BoardRuntime b = board(0.0f, Vector3::ZERO);
    const auto contacts = world.query(b, config());
    CHECK_EQ(contacts.size(), std::size_t(4));
    for (std::size_t index = 0; index < contacts.size(); ++index) {
        const BoardCollision& collision = contacts[index];
        CHECK_EQ(collision.body_a, CollisionBody::Board(BodyId_ORDER[index]));
        CHECK_EQ(collision.body_b, CollisionBody::StaticWorld());
        close(collision.contact.normal.y, 1.0f);
        close(collision.contact.position_on_a.y, 0.0f);
        close(collision.contact.position_on_b.y, 0.0f);
        CHECK_EQ(collision.contact.static_friction, 0.8f);
        CHECK_EQ(collision.contact.dynamic_friction, 0.6f);
        CHECK_EQ(collision.contact.restitution, 0.2f);
        CHECK_EQ(collision.contact.tag, 42u);
    }
}

S3_TEST(contact_query_uses_live_poses_and_rejects_air_remote_geometry_and_backfaces) {
    BoardWorld world = BoardWorld::new_({triangle()});
    BoardRuntime b = board(0.0f, Vector3::ZERO);
    CHECK_EQ(world.query(b, config()).size(), std::size_t(4));
    for (auto& body : b.bodies_mut()) body.rates.position.y += 1.0f;
    CHECK(world.query(b, config()).empty());
    for (auto& body : b.bodies_mut()) {
        body.rates.position.y -= 1.0f;
        body.rates.position.x += 30.0f;
    }
    CHECK(world.query(b, config()).empty());
    for (auto& body : b.bodies_mut()) {
        body.rates.position.x -= 30.0f;
        body.rates.position.y -= RETAIL_WHEEL_RADIUS * 1.5f;
    }
    CHECK(world.query(b, config()).empty());
}

S3_TEST(approaching_velocity_padding_and_world_fatness_control_real_contact_acceptance) {
    BoardWorld world = BoardWorld::new_({triangle()});
    BoardRuntime b = board(0.01f, Vector3::ZERO);
    WheelWorldSettings settings = config();
    settings.query.maximum_separating_distance = 0.05f;
    CHECK(world.query(b, settings).empty());
    for (auto& body : b.bodies_mut()) body.rates.linear_velocity.y = -1.0f;
    CHECK_EQ(world.query(b, settings).size(), std::size_t(4));
    settings.query.maximum_separating_distance = 0.004f;
    CHECK(world.query(b, settings).empty());
    for (auto& body : b.bodies_mut()) body.rates.linear_velocity.y = 1.0f;
    settings.query.volume_padding = 0.02f;
    CHECK_EQ(world.query(b, settings).size(), std::size_t(4));
    settings.query.volume_padding = 0.0f;
    WorldTriangle thick = triangle();
    thick.triangle.fatness = 0.02f;
    BoardWorld thick_world = BoardWorld::new_({thick});
    const auto contacts = thick_world.query(b, settings);
    CHECK_EQ(contacts.size(), std::size_t(4));
    close(contacts[0].contact.position_on_b.y, 0.02f);
}

S3_TEST(duplicate_triangles_and_explicit_contact_capacity_do_not_multiply_solver_rows) {
    const BoardRuntime b = board(0.0f, Vector3::ZERO);
    BoardWorld world = BoardWorld::new_({triangle(), triangle()});
    CHECK_EQ(world.query(b, config()).size(), std::size_t(4));
    CHECK_EQ(world.dropped_contacts(), 4u);
    WheelWorldSettings settings = config();
    settings.retention.capacity = 3;
    CHECK_EQ(world.query(b, settings).size(), std::size_t(3));
    CHECK(world.dropped_contacts() > 0);
    settings.retention.capacity = 100;
    settings.retention.deferred_reduction = true;
    BoardWorld repeated = BoardWorld::new_(std::vector<WorldTriangle>(8, triangle()));
    CHECK_EQ(repeated.query(b, settings).size(), std::size_t(4));
}

S3_TEST(world_contacts_feed_supporting_reactions_into_the_persistent_board_solver) {
    const Vector3 gravity{0.0f, -9.8f, 0.0f};
    BoardRuntime supported = board(0.0f, gravity);
    BoardRuntime falling = board(0.0f, gravity);
    BoardWorld world = BoardWorld::new_({triangle()});
    const auto collisions = world.query(supported, config());
    CHECK_EQ(collisions.size(), std::size_t(4));
    const BoardStepSettings settings{
        RetailSimulationStep::fixed_60_hz(30, 0.001f, gravity),
        25,
        default_truck_transforms(),
        retail_truck_drive_dynamics(RetailTruckDriveSettings::STOCK),
        0.0f,
    };
    supported.advance(collisions, {0.0f, 0.0f}, settings);
    falling.advance(std::vector<BoardCollision>{}, {0.0f, 0.0f}, settings);
    auto momentum = [](const BoardRuntime& b) {
        float s = -0.0f;
        for (const auto& body : b.bodies()) s += body.rates.linear_velocity.y / body.inertia.inverse_mass;
        return s;
    };
    CHECK(momentum(supported) > momentum(falling) + 0.01f);
    CHECK_EQ(supported.hook().body.rates.world_inverse_inertia, Basis3{});
}

S3_TEST(host_geometry_rejects_degenerate_triangles) {
    CHECK(!WorldTriangle::from_vertices({Vector3::ZERO, Vector3::ZERO, Vector3::ZERO}, material(0.0f, 0.0f, 0.0f), 0,
                                        0, {0.0f, 0.0f, 0.0f}, 0.0f)
               .has_value());
}

S3_TEST(attached_volume_contact_reaches_its_actual_solver_body) {
    BoardRuntime b = board(0.0f, Vector3::ZERO);
    auto attached = b.bodies()[0];
    attached.rates.position = Vector3{0.0f, RETAIL_WHEEL_RADIUS, 0.0f};
    attached.rates.linear_velocity.y = -1.0f;
    const float before = attached.rates.linear_velocity.y;
    const BoardWorldVolume volume{
        CollisionBody::Attached(0),
        Sphere{attached.rates.position, RETAIL_WHEEL_RADIUS},
        attached.rates.linear_velocity,
        material(0.0f, 0.0f, 0.0f),
    };
    BoardWorld world = BoardWorld::new_({triangle(), triangle()});
    const WheelWorldSettings cfg = config();
    const std::vector<BoardWorldVolume> volumes{volume};
    const auto contacts = world.query_primitives(volumes, cfg.query, cfg.retention);
    CHECK_MSG(contacts.size() == 1, "retention must preserve attached body identity");
    CHECK_EQ(contacts[0].body_a, CollisionBody::Attached(0));
    const BoardStepSettings settings{
        RetailSimulationStep::fixed_60_hz(30, 0.001f, Vector3::ZERO),
        25,
        default_truck_transforms(),
        retail_truck_drive_dynamics(RetailTruckDriveSettings::STOCK),
        0.0f,
    };
    AttachedStep step;
    step.bodies = {&attached};
    b.advance_attached(contacts, {0.0f, 0.0f}, settings, step);
    CHECK(attached.rates.linear_velocity.y > before + 0.5f);
    CHECK_MSG(b.contact_reports().empty(), "skater contacts are not board observations");
}

// ---------------- board_world/tests.rs ----------------

RetailContactMaterial material() { return RetailContactMaterial{0.7f, 0.4f, 0.2f}; }

WorldTriangle face(float x, std::uint32_t tag, float fatness) {
    return s3::rs::unwrap(WorldTriangle::from_vertices(
        {Vector3{x - 2.0f, 0.0f, -2.0f}, Vector3{x - 2.0f, 0.0f, 2.0f}, Vector3{x + 2.0f, 0.0f, -2.0f}}, material(),
        tag, 0xe0, {1.0f, 1.0f, 1.0f}, fatness));
}
WorldTriangle face_at(float x, float y, std::uint32_t tag, float fatness) {
    return s3::rs::unwrap(WorldTriangle::from_vertices(
        {Vector3{x - 2.0f, y, -2.0f}, Vector3{x - 2.0f, y, 2.0f}, Vector3{x + 2.0f, y, -2.0f}}, material(), tag,
        0xe0, {1.0f, 1.0f, 1.0f}, fatness));
}
BoardWorld annotated(std::vector<WorldTriangle> triangles) {
    std::vector<QueryMesh> meshes;
    for (std::size_t i = 0; i < triangles.size(); ++i) {
        QueryMesh m;
        m.triangle_range = Range{i, i + 1};
        m.local_to_world = RetailAffineTransform::IDENTITY;
        m.world_to_local = RetailAffineTransform::IDENTITY;
        m.local_bounds = s3::rs::unwrap(Bounds::from_points(triangles[i].triangle.vertices));
        m.matching_group = -1;
        m.rejection_flags = 0x2000;
        m.geometry = 5000 + static_cast<std::uint32_t>(i);
        m.pool = QueryPool::Ground;
        meshes.push_back(m);
    }
    std::vector<std::uint16_t> packed_surfaces(triangles.size(), 17);
    QueryMetadata metadata{packed_surfaces, meshes, {}, 0};
    return BoardWorld::with_query_metadata(std::move(triangles), std::move(metadata)).unwrap();
}
std::vector<WorldTriangle> tiled() {
    std::vector<WorldTriangle> out;
    for (std::uint32_t i = 0; i < 1024; ++i) {
        out.push_back(face(static_cast<float>((i * 37) % 1024) * 10.0f, i, 0.0f));
    }
    return out;
}

S3_TEST(zip_hierarchy_matches_inclusive_linear_mesh_scan_and_source_order) {
    const BoardWorld world = annotated(tiled());
    const auto& meshes = world.query_metadata().unwrap()->meshes;
    const query_index::QueryIndex index(meshes);
    for (float x : {-100.0f, -2.0f, 0.0f, 2.0f, 8.0f, 10.0f, 155.0f, 10230.0f, 10300.0f}) {
        for (float radius : {0.0f, 2.0f, 31.0f, 20000.0f}) {
            const Bounds bounds = s3::rs::unwrap(Bounds::from_points({Vector3{x, 0.0f, 0.0f}})).expanded(radius);
            std::vector<std::size_t> expected;
            for (std::size_t i = 0; i < meshes.size(); ++i) {
                if (meshes[i].local_bounds.overlaps(bounds)) expected.push_back(i);
            }
            CHECK(index.query(bounds, meshes) == expected);
        }
    }
}

S3_TEST(line_candidates_prune_distant_clusters_and_keep_canonical_indices) {
    const BoardWorld world = annotated(tiled());
    const Vector3 start{-1.0f, 1.0f, -1.0f};
    const Vector3 end{-1.0f, -1.0f, -1.0f};
    std::vector<std::size_t> indices;
    for (const auto& [i, t] : world.line_candidates(start, end, 0.0f)) {
        (void)t;
        indices.push_back(i);
    }
    CHECK(indices == std::vector<std::size_t>{0});
    CHECK_EQ(world.query_metadata().unwrap()->meshes[0].geometry, 5000u);
    CHECK_EQ(world.query_metadata().unwrap()->meshes[0].rejection_flags, 0x2000u);
    CHECK_EQ(world.query_metadata().unwrap()->packed_surfaces[0], std::uint16_t(17));
    CHECK((world.candidate_ranges(std::nullopt) == std::vector<Range>{Range{0, 1024}}));
    const Bounds invalid{Vector3{f32::NAN_, 0.0f, 0.0f}, Vector3::ZERO};
    CHECK((world.candidate_ranges(invalid) == std::vector<Range>{Range{0, 1024}}));
    const BoardWorld empty = annotated({});
    CHECK_EQ(empty.line_candidates(start, end, 0.0f).size(), std::size_t(0));
}

S3_TEST(accelerated_lines_match_full_scan_with_fatness_and_equal_hits) {
    std::vector<WorldTriangle> triangles = tiled();
    triangles.insert(triangles.begin(), face(0.0f, 9000, 0.1f));
    triangles.insert(triangles.begin(), face(0.0f, 9001, 0.1f));
    const BoardWorld linear = BoardWorld::new_(triangles);
    const BoardWorld world = annotated(triangles);
    for (float x : {-100.0f, -2.1f, -1.0f, 0.0f, 2.0f, 9.0f, 509.0f, 10229.0f}) {
        for (float radius : {0.0f, 0.03f, 0.5f}) {
            const Vector3 start{x, 2.0f, -1.0f};
            const Vector3 end{x, -2.0f, -1.0f};
            CHECK(world.query_swept_line(start, end, radius) == linear.query_swept_line(start, end, radius));
        }
    }
    CHECK_EQ(s3::rs::unwrap(
                 world.query_thin_line(Vector3{-1.0f, 2.0f, -1.0f}, Vector3{-1.0f, -2.0f, -1.0f}).unwrap())
                 .tag,
             9001u);
}

S3_TEST(thin_endpoint_and_barycentric_tolerances_survive_broadphase) {
    const std::vector<WorldTriangle> triangles{face(0.0f, 7, 0.0f)};
    const BoardWorld linear = BoardWorld::new_(triangles);
    const BoardWorld world = annotated(triangles);
    const std::pair<Vector3, Vector3> cases[] = {
        {Vector3{-1.0f, 1.0f, -1.0f}, Vector3{-1.0f, 0.000005f, -1.0f}},
        {Vector3{-1.0f, -0.000005f, -1.0f}, Vector3{-1.0f, -1.0f, -1.0f}},
        {Vector3{-2.00002f, 1.0f, -1.0f}, Vector3{-2.00002f, -1.0f, -1.0f}},
    };
    for (const auto& [start, end] : cases) {
        const auto expected = linear.query_thin_line(start, end).unwrap();
        CHECK_MSG(expected.has_value(), "fixture must exercise the thin-leaf tolerance");
        CHECK(world.query_thin_line(start, end).unwrap() == expected);
    }
}

S3_TEST(predictive_contacts_and_retention_match_full_scan_for_every_primitive) {
    // The distant tiles are staggered in Y on purpose. `tiled()` puts all 1024
    // faces in the plane y=0, and the recovered candidate producer82ACEA30
    // offers only the triangle's own face normal for point/segment/triangle
    // volumes, so a coplanar tile 10km away still projects to a near-zero
    // separation and survives82ACE968's `separation > fat + limit` gate. That
    // is native behaviour: the native narrow phase is only ever reached through
    // the broadphase, which rejects laterally. Feeding it an unculled full scan
    // therefore measures the narrow phase outside its contract, not the
    // hierarchy. Separating the tiles along the one axis the SAT does test
    // makes the linear scan a valid oracle for the accelerated traversal.
    std::vector<WorldTriangle> triangles;
    {
        const auto tiles = tiled();
        for (std::size_t i = 0; i < tiles.size(); ++i) {
            const float x = tiles[i].triangle.vertices[0].x + 2.0f;
            const float y = x == 0.0f ? 0.0f : 5.0f + static_cast<float>(i);
            triangles.push_back(face_at(x, y, tiles[i].tag, 0.0f));
        }
    }
    triangles.insert(triangles.begin(), face(0.0f, 9000, 0.02f));
    triangles.insert(triangles.begin(), face(0.0f, 9001, 0.02f));
    BoardWorld linear = BoardWorld::new_(triangles);
    BoardWorld world = annotated(triangles);
    const Vector3 center{-0.7f, 0.25f, -0.7f};
    const ContactPrimitive primitives[] = {
        ContactPrimitive(Sphere{center, 0.2f}),
        ContactPrimitive(ContactPrimitive::Capsule{center, Vector3{1.0f, 0.0f, 0.0f}, 0.3f, 0.2f}),
        ContactPrimitive(ContactPrimitive::RoundedBox{
            center, Basis3{{{{0.6f, 0.0f, 0.8f}, {0.0f, 1.0f, 0.0f}, {-0.8f, 0.0f, 0.6f}}}},
            Vector3{0.3f, 0.2f, 0.1f}, 0.02f}),
        ContactPrimitive(triangle_from_volume(
            {Vector3{-1.0f, 0.1f, -1.0f}, Vector3{-1.0f, 0.1f, 0.0f}, Vector3{0.0f, 0.1f, -1.0f}}, 0.03f,
            {1.0f, 1.0f, 1.0f}, 0xe0)),
    };
    bool observed = false;
    for (float velocity : {-60.0f, -1.0f, 0.0f, 1.0f}) {
        std::vector<BoardWorldVolume> volumes;
        for (std::size_t i = 0; i < 4; ++i) {
            volumes.push_back(BoardWorldVolume{CollisionBody::Attached(i), primitives[i],
                                               Vector3{0.0f, velocity, 0.0f}, material()});
        }
        const std::pair<float, float> cases[] = {{0.0f, 0.0f}, {0.0f, 0.5f}, {0.2f, 0.0f}, {0.02f, 0.5f}};
        for (const auto& [padding, maximum] : cases) {
            const WorldContactSettings query{padding, maximum, -1.0f, 0.0f, false};
            for (std::uint32_t capacity : {1u, 3u, 100u}) {
                for (bool deferred_reduction : {false, true}) {
                    const ContactRetentionSettings retention{capacity, 0.000001f, deferred_reduction};
                    auto snapshot = [](s3::physics::Slice<const BoardCollision> hits) {
                        std::vector<ContactRecord> out;
                        for (const auto& h : hits) out.push_back(retention_record(h.body_a, h.contact));
                        return out;
                    };
                    const auto expected = snapshot(linear.query_primitives(volumes, query, retention));
                    observed = observed || !expected.empty();
                    const auto actual = snapshot(world.query_primitives(volumes, query, retention));
                    CHECK_MSG(actual == expected, "vel=" << velocity << " pad=" << padding << " max=" << maximum
                                                         << " cap=" << capacity << " defer=" << deferred_reduction);
                    CHECK_EQ(world.dropped_contacts(), linear.dropped_contacts());
                }
            }
        }
    }
    CHECK(observed);
}
}  // namespace
