// Port of skate-game/src/physics/player_input/grind/world.rs.
//! Static host for original S3 82C20728 and 82E0AAD0 line batches.
//! Both use scene mask7, matching ID2952, side flags3, rejection mask0 and
//! a null surface-exclusion table. Metadata is mandatory, never render tags.
#include "s3/game/physics/player_input/grind/world.hpp"

#include <algorithm>
#include <cmath>

#include "s3/core/physics/board_world/query_metadata.hpp"
#include "s3/core/physics/triangle_query.hpp"

namespace s3::game::physics::player_input::grind::world {

using sc::math::Vector3;
using sc::physics::board_world::query_metadata::Bounds;
using sc::physics::board_world::query_metadata::QueryPool;
using sc::physics::triangle_query::triangle_segment;
using sc::physics::triangle_query::TriangleLineHit;

rs::Result<std::optional<ProbeHit>, std::string> surface_probe(const BoardWorld& world,
                                                               std::array<std::uint32_t, 2> actor, std::size_t index,
                                                               Probe probe) {
    if (index >= 7) {
        return rs::make_err(fmt::cat("Grind surface descriptor ", index, " is outside native batch7"));
    }
    return line(world, static_cast<std::int32_t>(actor[1]), probe);
}

rs::Result<std::optional<ForceExitHit>, std::string> force_exit_line(const BoardWorld& world,
                                                                     std::array<std::uint32_t, 2> actor,
                                                                     ForceExitProbe probe) {
    S3G_TRY(const auto hit, line(world, static_cast<std::int32_t>(actor[1]), Probe{probe.start, probe.end, 0.0f}));
    if (!hit) return std::optional<ForceExitHit>{};
    return std::optional<ForceExitHit>(ForceExitHit{hit->normal});
}

rs::Result<std::optional<ProbeHit>, std::string> line(const BoardWorld& world, std::int32_t matching, Probe probe) {
    S3G_TRY(const auto* metadata, world.query_metadata());
    if (!std::isfinite(probe.radius) || probe.radius < 0.0f) {
        return rs::make_err(std::string("Invalid grind line radius"));
    }
    const Vector3 start = vector(probe.start);
    const Vector3 end = vector(probe.end);
    const auto raw_bounds = Bounds::from_points(std::array<Vector3, 2>{start, end});
    if (!raw_bounds) {
        return rs::make_err(std::string("Non-finite grind line endpoints"));
    }
    const Bounds bounds = raw_bounds->expanded(probe.radius);
    for (const float v : {bounds.min.x, bounds.min.y, bounds.min.z, bounds.max.x, bounds.max.y, bounds.max.z}) {
        if (!std::isfinite(v)) {
            return rs::make_err(std::string("Grind line bounds overflow"));
        }
    }
    const Vector3 delta = sub(end, start);
    // 82770650's component gate precedes dispatch, including rounded lines.
    const float threshold = rs::from_bits(0x37800000u);
    if (!(std::fabs(delta.x) > threshold || std::fabs(delta.y) > threshold || std::fabs(delta.z) > threshold)) {
        return std::optional<ProbeHit>{};
    }
    // The index is conservative; native exact gates below remain authoritative.
    S3G_TRY(const auto candidates, world.candidate_mesh_indices(bounds));
    // Current BoardWorld applies its stored per-triangle bounds filter after
    // cluster culling. Collect once in canonical order, then restrict each mesh
    // to its candidate slice instead of walking its entire triangle range.
    const auto triangle_candidates = world.line_candidates(start, end, probe.radius);
    float nearest = rs::f32::MAX;
    std::optional<ProbeHit> output;
    // 82764AB0 mask7 and8276EC40 aggregate nearest: ties retain earlier pools.
    for (const QueryPool pool : {QueryPool::Ground, QueryPool::Island, QueryPool::Conditional}) {
        if (pool == QueryPool::Conditional && metadata->island_flags != 3) {
            continue;
        }
        for (const std::size_t mesh_index : candidates) {
            const auto& mesh = metadata->meshes[mesh_index];
            if (mesh.pool != pool ||
                !(matching == -1 || mesh.matching_group == -1 || matching == mesh.matching_group) ||
                !bounds.overlaps(mesh.local_bounds)) {
                continue;
            }
            // Request rejection mask0 deliberately accepts every rejection_flags
            // value; null exclusion table accepts every authored packed code.
            // BoardWorld validates identity transforms and world-space vertices.
            const auto first = std::partition_point(
                triangle_candidates.begin(), triangle_candidates.end(),
                [&](const auto& c) { return c.first < mesh.triangle_range.start; });
            const auto last = std::partition_point(
                triangle_candidates.begin(), triangle_candidates.end(),
                [&](const auto& c) { return c.first < mesh.triangle_range.end; });
            for (auto it = first; it < last; ++it) {
                const std::size_t triangle_index = it->first;
                const auto vertices = it->second->triangle.vertices;
                const auto triangle_bounds = Bounds::from_points(vertices);
                if (!triangle_bounds) {
                    return rs::make_err(std::string("Non-finite authored grind triangle"));
                }
                // 82ACA170 decoder's per-volume AABB gate; no leaf tolerance pad.
                if (!bounds.overlaps(*triangle_bounds)) {
                    continue;
                }
                TriangleLineHit hit{Vector3::ZERO, Vector3::ZERO, 0.0f, {0.0f, 0.0f, 0.0f}};
                // 827719B8 explicitly passes zero TRIANGLE fatness, irrespective
                // of the contact triangle's stored fatness. Not BoardWorld.query.
                if (!triangle_segment(hit, start, delta, vertices, probe.radius, 0.0f)) {
                    continue;
                }
                const float fraction = clamp_fraction(hit.fraction);
                if (fraction < nearest) {
                    nearest = fraction;
                    output = ProbeHit{
                        fraction,
                        packed(hit.position),
                        // Decoded volume+16, NOT the rounded leaf's contact normal.
                        packed(face(vertices)),
                        static_cast<std::uint32_t>(metadata->packed_surfaces[triangle_index]),
                    };
                }
            }
        }
    }
    return output;
}

sc::math::Vector3 vector(std::array<float, 4> v) { return Vector3::new_(v[0], v[1], v[2]); }

std::array<float, 4> packed(sc::math::Vector3 v) { return {v.x, v.y, v.z, 0.0f}; }

sc::math::Vector3 sub(sc::math::Vector3 a, sc::math::Vector3 b) { return Vector3::new_(a.x - b.x, a.y - b.y, a.z - b.z); }

float clamp_fraction(float fraction) {
    const float lower = -fraction >= 0.0f ? 0.0f : fraction;
    return 1.0f - lower >= 0.0f ? lower : 1.0f;
}

sc::math::Vector3 face(std::array<sc::math::Vector3, 3> vertices) {
    const Vector3 a = vertices[0], b = vertices[1], c = vertices[2];
    const Vector3 u = sub(b, a);
    const Vector3 v = sub(c, a);
    const Vector3 n = Vector3::new_(rs::mul_add(-u.z, v.y, u.y * v.z), rs::mul_add(-u.x, v.z, u.z * v.x),
                                    rs::mul_add(-u.y, v.x, u.x * v.y));
    const float q = rs::mul_add(n.z, n.z, rs::mul_add(n.y, n.y, n.x * n.x));
    float r = rs::recip(rs::sqrt(q));
    for (int i = 0; i < 2; ++i) {
        r = rs::mul_add(r * 0.5f, rs::mul_add(-q, r * r, 1.0f), r);
    }
    return Vector3::new_(n.x * r, n.y * r, n.z * r);
}

}  // namespace s3::game::physics::player_input::grind::world
