// Port of skate-game/src/physics/offboard/contact_toolkit/world.rs
#include "s3/game/physics/offboard/contact_toolkit/world.hpp"

#include <algorithm>
#include <cinttypes>
#include <cstdio>

namespace s3::game::physics::offboard::contact_toolkit::world {

using sc::air::trajectory::SurfaceHit;
using sc::math::Vector3;
using sc::physics::triangle_query::TriangleLineHit;
using sc::physics::triangle_query::triangle_segment;
using Frame = sc::player::offboard::contact_toolkit::Frame;
namespace ground = ::s3::player::offboard::ground_query;

namespace {
Vector vector4(const std::array<float, 3>& v) { return {v[0], v[1], v[2], 0.0f}; }
bool matches(std::int32_t a, std::int32_t b) { return a == -1 || b == -1 || a == b; }
Vector3 vec3(Vector v) { return Vector3::new_(v[0], v[1], v[2]); }
Vector lanes(Vector3 v) { return {v.x, v.y, v.z, 0.0f}; }
Vector sub(Vector a, Vector b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
float dot(Vector a, Vector b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vector cross(Vector a, Vector b) {
    return {
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
        0.0f,
    };
}
Vector face(const std::array<Vector, 3>& t) { return sc::player::offboard::contact_toolkit::triangle_normal(t); }
///82761698: full edge-cross-axis, coordinate-axis, and face-plane separation.
///AABB overlap alone must not select a face for normal refinement.
bool triangle_box(const std::array<Vector, 3>& vertices, Vector center, float radius) {
    const std::array<Vector, 3> v{sub(vertices[0], center), sub(vertices[1], center), sub(vertices[2], center)};
    const std::array<Vector, 3> edges{sub(v[1], v[0]), sub(v[2], v[1]), sub(v[0], v[2])};
    const std::array<Vector, 3> axes{Vector{1.0f, 0.0f, 0.0f, 0.0f}, Vector{0.0f, 1.0f, 0.0f, 0.0f},
                                     Vector{0.0f, 0.0f, 1.0f, 0.0f}};
    auto separated = [&](Vector axis) {
        const std::array<float, 3> p{dot(v[0], axis), dot(v[1], axis), dot(v[2], axis)};
        const float extent =
            rs::mul_add(rs::abs(axis[2]), radius, rs::mul_add(rs::abs(axis[1]), radius, rs::abs(axis[0]) * radius));
        return rs::min(rs::min(p[0], p[1]), p[2]) > extent || rs::max(rs::max(p[0], p[1]), p[2]) < -extent;
    };
    for (const auto& edge : edges) {
        for (const auto& axis : axes) {
            if (separated(cross(edge, axis))) return false;
        }
    }
    for (const auto& axis : axes) {
        if (separated(axis)) return false;
    }
    return !separated(cross(edges[0], edges[1]));
}
std::size_t partition_point(const std::vector<std::size_t>& c, std::size_t bound) {
    return static_cast<std::size_t>(std::lower_bound(c.begin(), c.end(), bound) - c.begin());
}
std::vector<std::size_t> candidate_indices(const BoardWorld& world, Vector3 start, Vector3 end, float radius) {
    std::vector<std::size_t> out;
    for (const auto& entry : world.line_candidates(start, end, radius)) out.push_back(entry.first);
    return out;
}
}  // namespace

rs::Result<std::vector<std::optional<ground::LineHit>>, const char*> StaticScene::lines(
    const std::vector<ground::Line>& requests, std::int32_t matching_group) const {
    std::vector<std::optional<ground::LineHit>> output(requests.size());
    for (QueryPool pool : pools()) {
        for (std::size_t i = 0; i < requests.size(); ++i) {
            const auto& request = requests[i];
            auto& destination = output[i];
            const LineProbe probe{lanes(request.start), lanes(request.end), request.radius};
            auto r = line(pool, probe, matching_group, 0);
            if (r.is_err()) return rs::make_err(r.unwrap_err());
            if (const auto& hit = r.unwrap()) {
                if (!destination.has_value() || hit->fraction < destination->fraction) {
                    destination = ground::LineHit{vec3(hit->position), vec3(hit->normal), hit->fraction, hit->surface};
                }
            }
        }
    }
    return output;
}

rs::Result<QueryResult, const char*> StaticScene::trajectory(QueryRequest request, std::int32_t matching_group,
                                                             std::uint32_t mesh_reject_mask) const {
    const auto t = request.trajectory;
    bool nonfinite = false;
    for (float v : t.position) nonfinite = nonfinite || !rs::is_finite(v);
    for (float v : t.velocity) nonfinite = nonfinite || !rs::is_finite(v);
    for (float v : t.acceleration) nonfinite = nonfinite || !rs::is_finite(v);
    if (!rs::is_finite(t.duration) || t.duration <= 0.0f || !rs::is_finite(request.radius) || request.radius <= 0.0f ||
        !rs::is_finite(request.start_error) || request.start_error <= 0.0f || !rs::is_finite(request.end_error) ||
        request.end_error <= 0.0f || nonfinite) {
        return rs::make_err("Invalid offboard accelerated-trajectory request");
    }
    QueryResult nearest = QueryResult::miss();
    for (QueryPool pool : pools()) {
        auto result_r = sc::air::trajectory::query_trajectory<const char*>(
            request,
            [&](Vector start, Vector end, float radius) -> rs::Result<std::optional<SurfaceHit>, const char*> {
                auto r = line(pool, LineProbe{start, end, radius}, matching_group, mesh_reject_mask);
                if (r.is_err()) return rs::make_err(r.unwrap_err());
                const auto& hit = r.unwrap();
                if (!hit) return std::optional<SurfaceHit>{};
                return std::optional<SurfaceHit>(
                    SurfaceHit{hit->position, hit->normal, hit->mesh_frame, std::uint32_t(hit->surface), hit->geometry});
            },
            [&](Vector center, float radius) { return nearby(pool, center, radius, matching_group); });
        if (result_r.is_err()) return rs::make_err(result_r.unwrap_err());
        const QueryResult result = result_r.unwrap();
        if (result.valid() && (!nearest.valid() || result.contact_time < nearest.contact_time)) {
            nearest = result;
        }
    }
    return nearest;
}

rs::Result<StaticScene, const char*> StaticScene::new_(const BoardWorld& world) {
    auto metadata = world.query_metadata();
    if (metadata.is_err()) return rs::make_err(metadata.unwrap_err());
    return StaticScene(&world, metadata.unwrap());
}

std::vector<QueryPool> StaticScene::pools() const {
    std::vector<QueryPool> out{QueryPool::Ground, QueryPool::Island};
    if (metadata->island_flags == 3) out.push_back(QueryPool::Conditional);
    return out;
}

rs::Result<std::optional<LineHit>, const char*> StaticScene::line(QueryPool pool, LineProbe probe, std::int32_t group,
                                                                  std::uint32_t reject) const {
    bool bad_xyz = false;
    for (int i = 0; i < 3; ++i) bad_xyz = bad_xyz || !rs::is_finite(probe.start[i]);
    for (int i = 0; i < 3; ++i) bad_xyz = bad_xyz || !rs::is_finite(probe.end[i]);
    if (!rs::is_finite(probe.radius) || probe.radius < 0.0f
        // The native packet has four SIMD lanes, but static collision and
        // query_sweep's dot3/length contract consume XYZ only. Lane 3 can
        // carry non-finite intermediate values (e.g. inf - inf) without
        // making the spatial segment invalid. Never relax XYZ/radius checks.
        || bad_xyz) {
        // Temporary audit-only error detail: preserve the rejected producer
        // packet and caller chain. Invalid spatial values are never substituted.
        char buf[512];
        std::snprintf(buf, sizeof buf,
                      "OFFBOARD_INVALID_SWEEP start=[%s, %s, %s, %s] end=[%s, %s, %s, %s] radius=%s "
                      "start_bits=[%08x, %08x, %08x, %08x] end_bits=[%08x, %08x, %08x, %08x] radius_bits=%08x "
                      "group=%d reject=%08x",
                      fmt::debug_f32(probe.start[0]).c_str(), fmt::debug_f32(probe.start[1]).c_str(),
                      fmt::debug_f32(probe.start[2]).c_str(), fmt::debug_f32(probe.start[3]).c_str(),
                      fmt::debug_f32(probe.end[0]).c_str(), fmt::debug_f32(probe.end[1]).c_str(),
                      fmt::debug_f32(probe.end[2]).c_str(), fmt::debug_f32(probe.end[3]).c_str(),
                      fmt::display_f32(probe.radius).c_str(), rs::to_bits(probe.start[0]), rs::to_bits(probe.start[1]),
                      rs::to_bits(probe.start[2]), rs::to_bits(probe.start[3]), rs::to_bits(probe.end[0]),
                      rs::to_bits(probe.end[1]), rs::to_bits(probe.end[2]), rs::to_bits(probe.end[3]),
                      rs::to_bits(probe.radius), group, reject);
        log::error(buf);
        return rs::make_err("Invalid offboard swept-line request");
    }
    const Vector delta = sub(probe.end, probe.start);
    bool any = false;
    for (int i = 0; i < 3; ++i) any = any || rs::abs(delta[i]) > rs::from_bits(0x37800000u);
    if (!any) return std::optional<LineHit>{};
    const Vector3 start = vec3(probe.start);
    const Vector3 direction = vec3(delta);
    float nearest = rs::f32::MAX;
    std::optional<LineHit> output;
    const auto bounds = world->line_candidate_bounds(start, vec3(probe.end), probe.radius);
    const std::vector<std::size_t> candidates = candidate_indices(*world, start, vec3(probe.end), probe.radius);
    auto meshes = world->candidate_mesh_indices(bounds);
    if (meshes.is_err()) return rs::make_err(meshes.unwrap_err());
    for (std::size_t mesh_index : meshes.unwrap()) {
        const auto& mesh = metadata->meshes[mesh_index];
        if (mesh.pool != pool || !matches(group, mesh.matching_group) || (mesh.rejection_flags & reject) != 0) {
            continue;
        }
        const std::size_t first = partition_point(candidates, mesh.triangle_range.start);
        const std::size_t last = partition_point(candidates, mesh.triangle_range.end);
        for (std::size_t k = first; k < last; ++k) {
            const std::size_t index = candidates[k];
            const auto vertices = world->triangles()[index].triangle.vertices;
            TriangleLineHit hit{Vector3::ZERO, Vector3::ZERO, 0.0f, {0.0f, 0.0f, 0.0f}};
            if (!triangle_segment(hit, start, direction, vertices, probe.radius, 0.0f)) continue;
            const float lower = -hit.fraction >= 0.0f ? 0.0f : hit.fraction;
            const float fraction = 1.0f - lower >= 0.0f ? lower : 1.0f;
            if (fraction < nearest) {
                nearest = fraction;
                const auto transform = mesh.local_to_world;
                output = LineHit{
                    lanes(hit.position),
                    face({lanes(vertices[0]), lanes(vertices[1]), lanes(vertices[2])}),
                    fraction,
                    metadata->packed_surfaces[index],
                    Frame{vector4(transform.basis.columns[0]), vector4(transform.basis.columns[1]),
                          vector4(transform.basis.columns[2]), lanes(transform.translation)},
                    mesh.geometry,
                };
            }
        }
    }
    return output;
}

rs::Result<std::vector<std::array<Vector, 3>>, const char*> StaticScene::nearby(QueryPool pool, Vector center,
                                                                                float radius,
                                                                                std::int32_t group) const {
    std::vector<std::array<Vector, 3>> output;
    output.reserve(64);
    //82770E00 does not apply the trajectory mesh rejection mask here.
    const auto bounds = world->line_candidate_bounds(vec3(center), vec3(center), radius);
    const std::vector<std::size_t> candidates = candidate_indices(*world, vec3(center), vec3(center), radius);
    auto meshes = world->candidate_mesh_indices(bounds);
    if (meshes.is_err()) return rs::make_err(meshes.unwrap_err());
    for (std::size_t mesh_index : meshes.unwrap()) {
        const auto& mesh = metadata->meshes[mesh_index];
        if (mesh.pool != pool || !matches(group, mesh.matching_group)) continue;
        const std::size_t first = partition_point(candidates, mesh.triangle_range.start);
        const std::size_t last = partition_point(candidates, mesh.triangle_range.end);
        for (std::size_t k = first; k < last; ++k) {
            const auto& tv = world->triangles()[candidates[k]].triangle.vertices;
            const std::array<Vector, 3> vertices{lanes(tv[0]), lanes(tv[1]), lanes(tv[2])};
            if (triangle_box(vertices, center, radius)) {
                output.push_back(vertices);
                if (output.size() == 64) return output;
            }
        }
    }
    return output;
}

rs::Result<QueryResults, const char*> StaticScene::execute(const Batch& batch) const {
    std::array<QueryResult, 3> trajectories{QueryResult::miss(), QueryResult::miss(), QueryResult::miss()};
    std::vector<std::optional<LineHit>> lines(batch.lines.size());
    for (QueryPool pool : pools()) {
        //8276EE88 chooses strict smallest nonnegative time, retaining the
        //first pool on ties. Normal refinement belongs to that same pool.
        for (std::size_t i = 0; i < batch.trajectories.size(); ++i) {
            const auto& request = batch.trajectories[i];
            auto& output = trajectories[i];
            const Vector start = request.trajectory.position;
            Vector end{};
            for (std::size_t k = 0; k < 4; ++k) end[k] = start[k] + request.trajectory.velocity[k];
            auto result_r = sc::player::offboard::contact_toolkit::query_sweep<const char*>(
                LineProbe{start, end, request.radius},
                [&](LineProbe probe) { return line(pool, probe, batch.matching_group, batch.mesh_reject_mask); },
                [&](Vector center, float radius) { return nearby(pool, center, radius, batch.matching_group); });
            if (result_r.is_err()) return rs::make_err(result_r.unwrap_err());
            const QueryResult result = result_r.unwrap();
            if (result.valid() && (!output.valid() || result.contact_time < output.contact_time)) {
                output = result;
            }
        }
        for (std::size_t i = 0; i < batch.lines.size(); ++i) {
            auto r = line(pool, batch.lines[i], batch.matching_group, batch.mesh_reject_mask);
            if (r.is_err()) return rs::make_err(r.unwrap_err());
            if (const auto& hit = r.unwrap()) {
                if (!lines[i].has_value() || hit->fraction < lines[i]->fraction) lines[i] = hit;
            }
        }
    }
    return QueryResults{trajectories, std::move(lines), edges(batch)};
}

std::vector<std::array<Vector, 2>> StaticScene::edges(const Batch& batch) const {
    const auto input = batch.input;
    Vector center{};
    Vector extent{};
    for (std::size_t i = 0; i < 4; ++i) {
        center[i] = rs::mul_add(input.up[i], 0.29999998f, rs::mul_add(input.forward[i], 0.89999998f, input.position[i]));
        extent[i] = rs::mul_add(rs::abs(input.forward[i]), 0.89999998f,
                                rs::mul_add(rs::abs(input.up[i]), 1.1f, rs::abs(input.right[i]) * 0.037500001f));
    }
    std::vector<std::array<Vector, 2>> out;
    for (const auto& edge : metadata->static_edges) {
        const Vector min = lanes(edge.local_bounds.min);
        const Vector max = lanes(edge.local_bounds.max);
        bool overlap = true;
        for (std::size_t i = 0; i < 3 && overlap; ++i) {
            overlap = center[i] - extent[i] <= max[i] && min[i] <= center[i] + extent[i];
        }
        if (!overlap) continue;
        out.push_back({lanes(edge.start), lanes(edge.end)});
        if (out.size() == 40) break;
    }
    return out;
}

}  // namespace s3::game::physics::offboard::contact_toolkit::world
