// Port of skate-core/src/physics/board_world/query_index.rs.
// Static bounds hierarchy over authored query meshes. Results are sorted back
// into source order before narrow-phase, preserving equal-hit/contact order.
#pragma once
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "s3/core/physics/board_world/query_metadata.hpp"

namespace s3::physics::board_world::query_index {

using query_metadata::Bounds;
using query_metadata::QueryMesh;
using math::Vector3;
using query_metadata::Range;

/// pub(super)
class QueryIndex {
public:
    QueryIndex() = default;
    explicit QueryIndex(const std::vector<QueryMesh>& meshes);
    static QueryIndex new_(const std::vector<QueryMesh>& meshes) { return QueryIndex(meshes); }
    std::vector<std::size_t> query(Bounds bounds, const std::vector<QueryMesh>& meshes) const;

private:
    struct Node {
        Bounds bounds;
        std::optional<std::pair<std::size_t, std::size_t>> children;
        Range range;
    };
    std::size_t build(const std::vector<QueryMesh>& meshes, Range range);
    std::vector<Node> nodes_;
    std::vector<std::size_t> order_;
};

}  // namespace s3::physics::board_world::query_index
