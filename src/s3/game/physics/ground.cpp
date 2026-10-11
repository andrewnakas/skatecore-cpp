// Port of skate-game/src/physics/ground.rs.
//! Our level geometry, queried with the recovered physics/world calculations.
#include "s3/game/physics/ground.hpp"

#include <algorithm>
#include <limits>

#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/physics/world_contact.hpp"
#include "s3/game/grind_world.hpp"

namespace s3::game::physics::ground {

using sc::physics::board_world::WorldTriangle;
using sc::physics::board_world::query_metadata::Bounds;
using sc::physics::board_world::query_metadata::QueryMesh;
using sc::physics::board_world::query_metadata::QueryMetadata;
using sc::physics::board_world::query_metadata::QueryPool;
using sc::physics::collision::TriangleFeature;
using sc::physics::drive_frames::RetailAffineTransform;

namespace {
constexpr Vector3 v(float x, float y, float z) { return Vector3{x, y, z}; }
}  // namespace

Surfaces surfaces(Terrain self) {
    switch (self) {
        case Terrain::Flat: return flat_surfaces();
        case Terrain::Course: return surfaces();
    }
    return surfaces();
}

BoardWorld world(Terrain self, RetailContactMaterial material) {
    switch (self) {
        case Terrain::Flat: return flat_world(material);
        case Terrain::Course: return world(material);
    }
    return world(material);
}

Surfaces surfaces() {
    std::vector<Quad> floor{{
        v(-50.0f, FLOOR_HEIGHT, -50.0f),
        v(50.0f, FLOOR_HEIGHT, -50.0f),
        v(50.0f, FLOOR_HEIGHT, 50.0f),
        v(-50.0f, FLOOR_HEIGHT, 50.0f),
    }};
    const Quad top{
        v(-3.0f, HEIGHT, -4.0f),
        v(3.0f, HEIGHT, -4.0f),
        v(3.0f, HEIGHT, 4.0f),
        v(-3.0f, HEIGHT, 4.0f),
    };
    std::vector<Quad> platform{top};
    for (std::size_t i : {0, 1, 3}) {
        // Forward face opens directly onto the ramp.
        const Vector3 a = top[i];
        const Vector3 b = top[(i + 1) % 4];
        platform.push_back({b, a, v(a.x, FLOOR_HEIGHT, a.z), v(b.x, FLOOR_HEIGHT, b.z)});
    }

    // Full-width downhill route straight ahead from spawn. Ease both ends
    // into the horizontal surfaces so the crest is not a sharp launch edge.
    const std::size_t ramp_segments = 48;
    const float ramp_length = 12.0f;
    auto ramp_point = [&](std::size_t i) {
        const float t = float(i) / float(ramp_segments);
        const float blend = t * t * (3.0f - 2.0f * t);
        return std::make_pair(4.0f + ramp_length * t, HEIGHT + (FLOOR_HEIGHT - HEIGHT) * blend);
    };
    for (std::size_t i = 0; i < ramp_segments; ++i) {
        const auto [z0, y0] = ramp_point(i);
        const auto [z1, y1] = ramp_point(i + 1);
        platform.push_back({
            v(-3.0f, y0, z0),
            v(3.0f, y0, z0),
            v(3.0f, y1, z1),
            v(-3.0f, y1, z1),
        });
    }
    {
        const auto rails = ::s3::game::grind_world::surfaces();
        platform.insert(platform.end(), rails.begin(), rails.end());
    }
    // Open ends at z +/-6 allow entry from the landing floor. Circular
    // transitions meet the four-metre flat bottom tangentially.
    const float radius = 2.5f;
    const std::uint32_t segments = 32;
    std::vector<std::pair<float, float>> profile;
    for (std::uint32_t k = 0; k <= segments; ++k) {
        const std::uint32_t i = segments - k;
        const float angle = rs::f32::consts::FRAC_PI_2 * float(i) / float(segments);
        profile.emplace_back(12.0f - 2.0f - radius * std::sin(angle),
                             FLOOR_HEIGHT + radius * (1.0f - std::cos(angle)));
    }
    for (std::uint32_t i = 0; i <= segments; ++i) {
        const float angle = rs::f32::consts::FRAC_PI_2 * float(i) / float(segments);
        profile.emplace_back(12.0f + 2.0f + radius * std::sin(angle),
                             FLOOR_HEIGHT + radius * (1.0f - std::cos(angle)));
    }
    std::vector<Quad> half_pipe;
    for (std::size_t k = 0; k + 1 < profile.size(); ++k) {
        const auto [x0, y0] = profile[k];
        const auto [x1, y1] = profile[k + 1];
        // The flat bottom already belongs to the landing floor.
        if (y0 == FLOOR_HEIGHT && y1 == FLOOR_HEIGHT) {
            continue;
        }
        half_pipe.push_back({
            v(x0, y0, -6.0f),
            v(x1, y1, -6.0f),
            v(x1, y1, 6.0f),
            v(x0, y0, 6.0f),
        });
    }
    // One-metre decks at both lips.
    for (const auto& [x0, x1] : {std::make_pair(6.5f, 7.5f), std::make_pair(16.5f, 17.5f)}) {
        const float y = FLOOR_HEIGHT + radius;
        half_pipe.push_back({v(x0, y, -6.0f), v(x1, y, -6.0f), v(x1, y, 6.0f), v(x0, y, 6.0f)});
    }
    // A separate 2.2 m ledge on the left of the course makes the custom
    // climbing flow directly testable without altering the riding routes.
    const Quad climb_top{
        v(-9.0f, FLOOR_HEIGHT + 2.2f, -2.0f),
        v(-5.0f, FLOOR_HEIGHT + 2.2f, -2.0f),
        v(-5.0f, FLOOR_HEIGHT + 2.2f, 2.0f),
        v(-9.0f, FLOOR_HEIGHT + 2.2f, 2.0f),
    };
    std::vector<Quad> climbing{climb_top};
    for (std::size_t i = 0; i < 4; ++i) {
        const Vector3 a = climb_top[i];
        const Vector3 b = climb_top[(i + 1) % 4];
        climbing.push_back({b, a, v(a.x, FLOOR_HEIGHT, a.z), v(b.x, FLOOR_HEIGHT, b.z)});
    }
    return {std::move(floor), std::move(platform), std::move(half_pipe), std::move(climbing)};
}

BoardWorld world(RetailContactMaterial material) { return build_world(material, surfaces(), query_edges()); }

Surfaces flat_surfaces() {
    return {
        std::vector<Quad>{{
            v(-50.0f, HEIGHT, -50.0f),
            v(50.0f, HEIGHT, -50.0f),
            v(50.0f, HEIGHT, 50.0f),
            v(-50.0f, HEIGHT, 50.0f),
        }},
        std::vector<Quad>{},
        std::vector<Quad>{},
        std::vector<Quad>{},
    };
}

BoardWorld flat_world(RetailContactMaterial material) { return build_world(material, flat_surfaces(), {}); }

BoardWorld build_world(RetailContactMaterial material, Surfaces surfaces_, std::vector<EdgeSegment> edges) {
    std::vector<WorldTriangle> triangles;
    const auto rail_faces = ::s3::game::grind_world::surfaces();
    for (std::size_t tag = 0; tag < surfaces_.size(); ++tag) {
        for (const auto& vertices : surfaces_[tag]) {
            const bool rail_face = std::find(rail_faces.begin(), rail_faces.end(), vertices) != rail_faces.end();
            const std::array<std::array<std::size_t, 3>, 2> halves{{{0, 2, 1}, {0, 3, 2}}};
            for (std::size_t half = 0; half < 2; ++half) {
                const auto& indices = halves[half];
                const std::uint32_t convex = rail_face ? (half == 0 ? 0x60u : 0xc0u) : 0u;
                WorldTriangle entry{};
                entry.triangle = sc::physics::world_contact::triangle_from_volume(
                    {vertices[indices[0]], vertices[indices[1]], vertices[indices[2]]}, 0.0f, {1.0f, 1.0f, 1.0f},
                    TriangleFeature::ONE_SIDED | convex);
                entry.material = material;
                entry.tag = static_cast<std::uint32_t>(tag);
                triangles.push_back(entry);
            }
        }
    }
    //This test level explicitly authors material0/category0/upper flags0.
    //This is host metadata, not a claim that the stock material is concrete,
    //and not derived from the neutral contact coefficients or rendering tag.
    std::vector<Vector3> points;
    points.reserve(triangles.size() * 3);
    for (const auto& entry : triangles) {
        for (const auto& p : entry.triangle.vertices) points.push_back(p);
    }
    const auto bounds = Bounds::from_points(points);
    if (!bounds) S3_PANIC("Authored level contains collision triangles");
    QueryMetadata metadata;
    metadata.packed_surfaces = std::vector<std::uint16_t>(triangles.size(), 0);
    QueryMesh mesh;
    mesh.triangle_range = {0, triangles.size()};
    mesh.local_to_world = RetailAffineTransform::IDENTITY;
    mesh.world_to_local = RetailAffineTransform::IDENTITY;
    mesh.local_bounds = *bounds;
    mesh.matching_group = -1;
    // This authored static collision mesh accepts trajectory queries.
    mesh.rejection_flags = 0;
    mesh.geometry = 1;
    mesh.pool = QueryPool::Ground;
    metadata.meshes.push_back(mesh);
    metadata.static_edges = std::move(edges);
    //No island or conditional meshes exist in the current static level.
    metadata.island_flags = 0;
    auto world_ = BoardWorld::with_query_metadata(std::move(triangles), std::move(metadata));
    if (world_.is_err()) S3_PANIC("Authored level query metadata matches its collision geometry");
    return std::move(world_.unwrap());
}

std::vector<EdgeSegment> query_edges() {
    const Quad top{
        v(-3.0f, HEIGHT, -4.0f),
        v(3.0f, HEIGHT, -4.0f),
        v(3.0f, HEIGHT, 4.0f),
        v(-3.0f, HEIGHT, 4.0f),
    };
    std::vector<std::pair<Vector3, Vector3>> pairs;
    for (std::size_t i : {0, 1, 3}) pairs.emplace_back(top[i], top[(i + 1) % 4]);
    const float lip_height = FLOOR_HEIGHT + 2.5f;
    for (float x : {7.5f, 16.5f}) {
        pairs.emplace_back(v(x, lip_height, -6.0f), v(x, lip_height, 6.0f));
    }
    std::vector<EdgeSegment> edges;
    for (const auto& [start, end] : pairs) {
        const auto b = Bounds::from_points({start, end});
        if (!b) S3_PANIC("Two authored edge endpoints");
        edges.push_back(EdgeSegment{start, end, *b});
    }
    return edges;
}

std::pair<sc::physics::collision::WorldContactSettings, sc::physics::board_world::ContactRetentionSettings>
query_settings() {
    sc::physics::collision::WorldContactSettings w;
    // Serialized primitive82DC3AE4/82DC3B84, source literal82165A00.
    w.volume_padding = 0.05f;
    // Ground job8277C864/8277C86C; board selection82768728.
    w.maximum_separating_distance = 0.5f;
    w.edge_cos_bend_normal_threshold = 0.999f;
    w.convexity_epsilon = 0.01f;
    w.is_object = false;
    sc::physics::board_world::ContactRetentionSettings r;
    r.capacity = std::numeric_limits<std::uint32_t>::max();  // Host storage capacity, no artificial row limit.
    // Step_Collision1 8276430C ->827771E8 ->82777384 ->8277C664.
    r.duplicate_distance_squared = -1.0f;
    r.deferred_reduction = true;
    return {w, r};
}

}  // namespace s3::game::physics::ground
