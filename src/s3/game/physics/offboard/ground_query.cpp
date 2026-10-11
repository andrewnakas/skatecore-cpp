// Port of skate-game/src/physics/offboard/ground_query.rs (+ edges.rs, lines.rs, transform.rs, world.rs)
#include "s3/game/physics/offboard/ground_query.hpp"

#include <algorithm>

namespace s3::game::physics::offboard::ground_query {

// ---- impl GroundQueryScene for Scene ----
rs::Result<std::vector<Edge>, const char*> Scene::edge_candidates(const EdgeSearch& search) {
    return edges::candidates(*this, search);
}
rs::Result<std::array<std::optional<LineHit>, 7>, const char*> Scene::query_lines(const GroundQueryPacket& packet) {
    return lines::query(*this, packet);
}

// ---- edges.rs ----
namespace edges {
namespace {
void append(std::vector<Edge>& output, Slice<const Segment> segments, const Frame& frame, Bounds query) {
    for (const auto& segment : segments) {
        if (output.size() == 40) break;
        if (transform::overlaps(transform::bounds(frame, segment.local_bounds), query)) {
            output.push_back(Edge{transform::point(frame, segment.edge.start), transform::point(frame, segment.edge.end)});
        }
    }
}
void body(std::vector<Edge>& output, const EdgeBody& body, Bounds query) {
    if (transform::overlaps(transform::bounds(body.local_to_world, body.local_bounds), query)) {
        append(output, body.segments, body.local_to_world, query);
    }
}
}  // namespace

std::vector<Edge> candidates(const Scene& scene, const EdgeSearch& search) {
    const Bounds query{search.min, search.max};
    std::vector<Edge> output;
    output.reserve(40);
    append(output, scene.static_edges, Frame::IDENTITY, query);
    if (const auto* normal = std::get_if<PrimaryEdges::Normal>(&scene.primary_edges.value)) {
        const Vector3 center = transform::scale(transform::add(query.min, query.max), 0.5f);
        for (const auto& provider : {normal->dynamic, normal->vehicles}) {
            for (const auto& entry : provider) {
                const Vector3 d = transform::sub(entry.local_to_world.position, center);
                const float squared = rs::mul_add(d.z, d.z, rs::mul_add(d.y, d.y, d.x * d.x));
                if (squared >= 225.0f) continue;
                //82C4C4E0 checks individual segment bounds, no asset bound gate.
                append(output, entry.segments, entry.local_to_world, query);
                if (output.size() == 40) break;
            }
        }
    } else if (const auto* alternate = std::get_if<PrimaryEdges::Alternate>(&scene.primary_edges.value)) {
        for (const auto& record : alternate->records) {
            if (!record.enabled) continue;
            const std::size_t choice = (search.context.selection_flags_2948 & 2u) != 0 ? 1 : 0;
            if (const auto& c = record.choices[choice]) {
                if (transform::matches(search.context.matching_id_2952, c->second)) {
                    body(output, *c->first, query);
                }
            }
        }
    }
    std::vector<const IndexedEdgeBody*> ordered;
    for (const auto& entry : scene.indexed_edges) ordered.push_back(&entry);
    // `sort_by_key` is stable.
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const IndexedEdgeBody* a, const IndexedEdgeBody* b) { return a->id < b->id; });
    for (const auto* entry : ordered) {
        if (!entry->disabled) body(output, *entry->body, query);
    }
    return output;
}
}  // namespace edges

// ---- lines.rs ----
namespace lines {
rs::Result<std::array<std::optional<LineHit>, 7>, const char*> query(const Scene& scene,
                                                                     const GroundQueryPacket& packet) {
    const std::array<Slice<const Mesh>, 3> pools{
        scene.ground_pool, scene.island_pool, scene.island_flags == 3 ? scene.conditional_pool : Slice<const Mesh>{}};
    return query_pools(pools, packet);
}

rs::Result<std::array<std::optional<LineHit>, 7>, const char*> query_pools(const std::array<Slice<const Mesh>, 3>& pools,
                                                                           const GroundQueryPacket& packet) {
    for (const auto& pool : pools) {
        for (const auto& mesh : pool) {
            if (mesh.triangles.size() != mesh.surfaces.size()) {
                return rs::make_err("Biped query mesh lacks exact per-triangle packed surfaces");
            }
        }
    }
    std::array<std::optional<LineHit>, 7> output{};
    for (std::size_t index = 0; index < packet.lines.size(); ++index) {
        const auto& line = packet.lines[index];
        if (!rs::is_finite(line.radius) || line.radius < 0.0f) return rs::make_err("Invalid Biped line radius");
        const Vector3 delta = transform::sub(line.end, line.start);
        const float threshold = rs::from_bits(0x37800000u);
        if (!(rs::abs(delta.x) > threshold || rs::abs(delta.y) > threshold || rs::abs(delta.z) > threshold)) continue;
        auto low = [&](float a, float b) { return rs::min(a, b) - line.radius; };
        auto high = [&](float a, float b) { return rs::max(a, b) + line.radius; };
        const Bounds bounds{
            Vector3::new_(low(line.start.x, line.end.x), low(line.start.y, line.end.y), low(line.start.z, line.end.z)),
            Vector3::new_(high(line.start.x, line.end.x), high(line.start.y, line.end.y),
                          high(line.start.z, line.end.z)),
        };
        float nearest = rs::f32::MAX;
        for (const auto& pool : pools) {
            for (const auto& mesh : pool) {
                if (!transform::matches(packet.context.matching_id_2952, mesh.matching_group) ||
                    !transform::overlaps(transform::bounds(mesh.world_to_local, bounds), mesh.local_bounds)) {
                    continue;
                }
                for (std::size_t t = 0; t < mesh.triangles.size(); ++t) {
                    const auto& triangle = mesh.triangles[t];
                    const std::uint16_t packed_surface = mesh.surfaces[t];
                    const auto& tv = triangle.triangle.vertices;
                    const std::array<Vector3, 3> vertices{transform::point(mesh.local_to_world, tv[0]),
                                                          transform::point(mesh.local_to_world, tv[1]),
                                                          transform::point(mesh.local_to_world, tv[2])};
                    sc::physics::triangle_query::TriangleLineHit hit{Vector3::ZERO, Vector3::ZERO, 0.0f,
                                                                     {0.0f, 0.0f, 0.0f}};
                    if (sc::physics::triangle_query::triangle_segment(hit, line.start, delta, vertices, line.radius,
                                                                      0.0f)) {
                        // Native outer leaf uses fsel, including its unordered choice.
                        const float lower = -hit.fraction >= 0.0f ? 0.0f : hit.fraction;
                        const float fraction = 1.0f - lower >= 0.0f ? lower : 1.0f;
                        if (fraction < nearest) {
                            nearest = fraction;
                            output[index] = LineHit{hit.position, transform::face(vertices), fraction, packed_surface};
                        }
                    }
                }
            }
        }
    }
    return output;
}
}  // namespace lines

// ---- transform.rs ----
namespace transform {
Vector3 add(Vector3 a, Vector3 b) { return Vector3::new_(a.x + b.x, a.y + b.y, a.z + b.z); }
Vector3 sub(Vector3 a, Vector3 b) { return Vector3::new_(a.x - b.x, a.y - b.y, a.z - b.z); }
Vector3 scale(Vector3 a, float s) { return Vector3::new_(a.x * s, a.y * s, a.z * s); }
Vector3 madd(Vector3 a, float s, Vector3 b) {
    return Vector3::new_(rs::mul_add(a.x, s, b.x), rs::mul_add(a.y, s, b.y), rs::mul_add(a.z, s, b.z));
}
Vector3 point(const Frame& f, Vector3 p) { return madd(f.forward, p.z, madd(f.up, p.y, madd(f.right, p.x, f.position))); }
Bounds bounds(const Frame& f, Bounds b) {
    const Vector3 c = point(f, scale(add(b.max, b.min), 0.5f));
    Vector3 e = scale(sub(b.max, b.min), 0.5f);
    auto abs = [](Vector3 v) { return Vector3::new_(rs::abs(v.x), rs::abs(v.y), rs::abs(v.z)); };
    e = madd(abs(f.forward), e.z, madd(abs(f.up), e.y, scale(abs(f.right), e.x)));
    return Bounds{sub(c, e), add(c, e)};
}
bool overlaps(Bounds a, Bounds b) {
    return a.min.x <= b.max.x && a.max.x >= b.min.x && a.min.y <= b.max.y && a.max.y >= b.min.y &&
           a.min.z <= b.max.z && a.max.z >= b.min.z;
}
bool matches(std::int32_t a, std::int32_t b) { return a == -1 || b == -1 || a == b; }
Vector3 face(const std::array<Vector3, 3>& v) {
    const Vector3 u = sub(v[1], v[0]);
    const Vector3 w = sub(v[2], v[0]);
    const Vector3 n = Vector3::new_(rs::mul_add(-u.z, w.y, u.y * w.z), rs::mul_add(-u.x, w.z, u.z * w.x),
                                    rs::mul_add(-u.y, w.x, u.x * w.y));
    const float q = rs::mul_add(n.z, n.z, rs::mul_add(n.y, n.y, n.x * n.x));
    float r = 1.0f / rs::sqrt(q);
    for (int i = 0; i < 2; ++i) r = rs::mul_add(r * 0.5f, rs::mul_add(-q, r * r, 1.0f), r);
    return scale(n, r);
}
}  // namespace transform

// ---- world.rs ----
namespace world {
rs::Result<std::vector<Edge>, const char*> WorldScene::edge_candidates(const EdgeSearch& search) {
    return edges.edge_candidates(search);
}
rs::Result<std::array<std::optional<LineHit>, 7>, const char*> WorldScene::query_lines(
    const GroundQueryPacket& packet) {
    auto metadata_r = world->query_metadata();
    if (metadata_r.is_err()) return rs::make_err(metadata_r.unwrap_err());
    const auto* metadata = metadata_r.unwrap();
    const auto& triangles = world->triangles();
    std::vector<std::size_t> candidates;
    for (const auto& line : packet.lines) {
        if (!rs::is_finite(line.radius) || line.radius < 0.0f) return rs::make_err("Invalid Biped line radius");
        auto r = world->candidate_mesh_indices(world->line_candidate_bounds(line.start, line.end, line.radius));
        if (r.is_err()) return rs::make_err(r.unwrap_err());
        const auto& v = r.unwrap();
        candidates.insert(candidates.end(), v.begin(), v.end());
    }
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
    std::array<std::vector<Mesh>, 3> pools;
    for (std::size_t index : candidates) {
        const auto& source = metadata->meshes[index];
        std::size_t pool = 0;
        switch (source.pool) {
            case sc::physics::board_world::query_metadata::QueryPool::Ground: pool = 0; break;
            case sc::physics::board_world::query_metadata::QueryPool::Island: pool = 1; break;
            case sc::physics::board_world::query_metadata::QueryPool::Conditional: pool = 2; break;
        }
        const auto& range = source.triangle_range;
        S3_ASSERT(range.start <= range.end && range.end <= triangles.size());
        S3_ASSERT(range.end <= metadata->packed_surfaces.size());
        pools[pool].push_back(Mesh{
            frame(source.local_to_world),
            frame(source.world_to_local),
            Bounds{source.local_bounds.min, source.local_bounds.max},
            source.matching_group,
            Slice<const WorldTriangle>(triangles.data() + range.start, range.end - range.start),
            Slice<const std::uint16_t>(metadata->packed_surfaces.data() + range.start, range.end - range.start),
        });
    }
    return lines::query_pools({Slice<const Mesh>(pools[0]), Slice<const Mesh>(pools[1]),
                               metadata->island_flags == 3 ? Slice<const Mesh>(pools[2]) : Slice<const Mesh>{}},
                              packet);
}

Frame frame(const sc::physics::drive_frames::RetailAffineTransform& f) {
    auto v = [](const std::array<float, 3>& a) { return Vector3::new_(a[0], a[1], a[2]); };
    return Frame{v(f.basis.columns[0]), v(f.basis.columns[1]), v(f.basis.columns[2]), f.translation};
}
}  // namespace world

}  // namespace s3::game::physics::offboard::ground_query
