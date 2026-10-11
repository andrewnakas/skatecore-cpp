// Port of skate-core/src/physics/board_world/query_metadata.rs.
#include "s3/core/physics/board_world/query_metadata.hpp"

#include "s3/core/physics/board_world.hpp"

namespace s3::physics::board_world::query_metadata {

namespace {
bool finite(Vector3 p) { return rs::is_finite(p.x) && rs::is_finite(p.y) && rs::is_finite(p.z); }
}  // namespace

std::optional<Bounds> Bounds::from_points(const Vector3* points, std::size_t count) {
    if (count == 0) return std::nullopt;
    const Vector3 first = points[0];
    if (!finite(first)) {
        return std::nullopt;
    }
    Bounds bounds{first, first};
    for (std::size_t i = 1; i < count; ++i) {
        const Vector3 p = points[i];
        if (!finite(p)) {
            return std::nullopt;
        }
        bounds.min = Vector3{rs::min(bounds.min.x, p.x), rs::min(bounds.min.y, p.y), rs::min(bounds.min.z, p.z)};
        bounds.max = Vector3{rs::max(bounds.max.x, p.x), rs::max(bounds.max.y, p.y), rs::max(bounds.max.z, p.z)};
    }
    return bounds;
}

bool Bounds::contains(Vector3 p) const {
    return finite(p) && p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y && p.z >= min.z &&
           p.z <= max.z;
}

bool Bounds::valid() const {
    return finite(min) && finite(max) && min.x <= max.x && min.y <= max.y && min.z <= max.z;
}

rs::Result<rs::Unit, const char*> QueryMetadata::validate(const std::vector<WorldTriangle>& triangles) const {
    if (packed_surfaces.size() != triangles.size()) {
        return rs::make_err("Query surface count must equal canonical triangle count");
    }
    std::size_t next = 0;
    for (const QueryMesh& mesh : meshes) {
        if (mesh.triangle_range.start != next || mesh.triangle_range.end <= next ||
            mesh.triangle_range.end > triangles.size()) {
            return rs::make_err("Query meshes must partition canonical triangles in supplied order");
        }
        // BoardWorld collision currently owns world-space static triangles.
        // Reject an unsupported scene transform rather than move query-only geometry.
        if (mesh.local_to_world != RetailAffineTransform::IDENTITY ||
            mesh.world_to_local != RetailAffineTransform::IDENTITY) {
            return rs::make_err("BoardWorld static query meshes require identity transforms");
        }
        if (!mesh.local_bounds.valid()) {
            return rs::make_err("Invalid query mesh bounds");
        }
        for (std::size_t i = mesh.triangle_range.start; i < mesh.triangle_range.end; ++i) {
            for (const Vector3& p : triangles[i].triangle.vertices) {
                if (!mesh.local_bounds.contains(p)) {
                    return rs::make_err("Query mesh bounds exclude canonical triangle geometry");
                }
            }
        }
        next = mesh.triangle_range.end;
    }
    if (next != triangles.size()) {
        return rs::make_err("Query metadata leaves canonical triangles unassigned");
    }
    for (const EdgeSegment& edge : static_edges) {
        if (!edge.local_bounds.valid() || !edge.local_bounds.contains(edge.start) ||
            !edge.local_bounds.contains(edge.end)) {
            return rs::make_err("Invalid authored query edge");
        }
    }
    return rs::Unit{};
}

}  // namespace s3::physics::board_world::query_metadata
