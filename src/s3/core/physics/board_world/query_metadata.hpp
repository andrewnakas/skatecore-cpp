// Port of skate-core/src/physics/board_world/query_metadata.rs.
// Canonical authored scene metadata; independent of rendering tags and actors.
#pragma once
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <vector>

#include "s3/core/math.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/rs.hpp"

namespace s3::physics::board_world {
struct WorldTriangle;
}

namespace s3::physics::board_world::query_metadata {

using drive_frames::RetailAffineTransform;
using math::Vector3;

/// Rust `std::ops::Range<usize>` (half-open).
struct Range {
    std::size_t start = 0;
    std::size_t end = 0;
    std::size_t len() const { return end > start ? end - start : 0; }
};
S3_DERIVE_EQ(Range, start, end)

enum class QueryPool : std::uint8_t {
    Ground,
    Island,
    Conditional,
};

struct Bounds {
    Vector3 min;
    Vector3 max;

    Bounds expanded(float padding) const {
        return Bounds{
            Vector3{min.x - padding, min.y - padding, min.z - padding},
            Vector3{max.x + padding, max.y + padding, max.z + padding},
        };
    }
    bool overlaps(const Bounds& other) const {
        return !(max.x < other.min.x || min.x > other.max.x || max.y < other.min.y || min.y > other.max.y ||
                 max.z < other.min.z || min.z > other.max.z);
    }
    static std::optional<Bounds> from_points(const Vector3* points, std::size_t count);
    template <std::size_t N>
    static std::optional<Bounds> from_points(const std::array<Vector3, N>& points) {
        return from_points(points.data(), N);
    }
    static std::optional<Bounds> from_points(const std::vector<Vector3>& points) {
        return from_points(points.data(), points.size());
    }
    static std::optional<Bounds> from_points(std::initializer_list<Vector3> points) {
        return from_points(points.begin(), points.size());
    }
    /// Rust-private.
    bool contains(Vector3 p) const;
    /// pub(super)
    bool valid() const;
};

struct EdgeSegment {
    Vector3 start;
    Vector3 end;
    Bounds local_bounds;
};

struct QueryMesh {
    Range triangle_range;
    RetailAffineTransform local_to_world;
    RetailAffineTransform world_to_local;
    Bounds local_bounds;
    std::int32_t matching_group = 0;
    /// S3 mesh184: trajectory rejection mask, separate from surface/material.
    std::uint32_t rejection_flags = 0;
    /// S3 mesh176 support/body identity; static registration uses0. This is
    /// separate from the host metadata index and authored mesh/unit names.
    std::uint32_t geometry = 0;
    QueryPool pool = QueryPool::Ground;
};

struct QueryMetadata {
    /// Exact authored uint16 codes, in canonical triangle order. Low7 material,
    /// next5 category, upper4 flags; never synthesized from WorldTriangle::tag.
    std::vector<std::uint16_t> packed_surfaces;
    std::vector<QueryMesh> meshes;
    /// Explicit level query edges; triangulation diagonals are not query edges.
    std::vector<EdgeSegment> static_edges;
    std::uint32_t island_flags = 0;

    /// pub(super)
    rs::Result<rs::Unit, const char*> validate(const std::vector<WorldTriangle>& triangles) const;
};

}  // namespace s3::physics::board_world::query_metadata
