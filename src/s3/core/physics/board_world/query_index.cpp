// Port of skate-core/src/physics/board_world/query_index.rs.
#include "s3/core/physics/board_world/query_index.hpp"

#include <algorithm>

namespace s3::physics::board_world::query_index {

QueryIndex::QueryIndex(const std::vector<QueryMesh>& meshes) {
    order_.resize(meshes.size());
    for (std::size_t i = 0; i < meshes.size(); ++i) order_[i] = i;
    if (!meshes.empty()) {
        build(meshes, Range{0, meshes.size()});
    }
}

std::size_t QueryIndex::build(const std::vector<QueryMesh>& meshes, Range range) {
    std::vector<Vector3> points;
    points.reserve(range.len() * 2);
    for (std::size_t k = range.start; k < range.end; ++k) {
        points.push_back(meshes[order_[k]].local_bounds.min);
        points.push_back(meshes[order_[k]].local_bounds.max);
    }
    const Bounds bounds = rs::unwrap(Bounds::from_points(points));
    const std::size_t id = nodes_.size();
    nodes_.push_back(Node{bounds, std::nullopt, range});
    if (range.len() > 8) {
        const float extents[3] = {
            bounds.max.x - bounds.min.x,
            bounds.max.y - bounds.min.y,
            bounds.max.z - bounds.min.z,
        };
        // Iterator::max_by returns the LAST maximum on ties.
        std::size_t axis = 0;
        for (std::size_t a = 1; a < 3; ++a) {
            if (rs::total_cmp(extents[a], extents[axis]) >= 0) axis = a;
        }
        auto center = [&](std::size_t i) {
            const Bounds b = meshes[i].local_bounds;
            const float low = (axis == 0 ? b.min.x : axis == 1 ? b.min.y : b.min.z);
            const float high = (axis == 0 ? b.max.x : axis == 1 ? b.max.y : b.max.z);
            return low * 0.5f + high * 0.5f;
        };
        const std::size_t mid = range.start + range.len() / 2;
        // select_nth_unstable_by with a total order (ties broken by index): the
        // partition sets are unique, so std::nth_element yields the same sets.
        std::nth_element(order_.begin() + static_cast<std::ptrdiff_t>(range.start),
                         order_.begin() + static_cast<std::ptrdiff_t>(mid),
                         order_.begin() + static_cast<std::ptrdiff_t>(range.end),
                         [&](std::size_t a, std::size_t b) {
                             const int c = rs::total_cmp(center(a), center(b));
                             return c != 0 ? c < 0 : a < b;
                         });
        const std::size_t left = build(meshes, Range{range.start, mid});
        const std::size_t right = build(meshes, Range{mid, range.end});
        nodes_[id].children = std::make_pair(left, right);
    }
    return id;
}

std::vector<std::size_t> QueryIndex::query(Bounds bounds, const std::vector<QueryMesh>& meshes) const {
    std::vector<std::size_t> result;
    if (nodes_.empty()) {
        return result;
    }
    std::vector<std::size_t> stack{0};
    while (!stack.empty()) {
        const std::size_t id = stack.back();
        stack.pop_back();
        const Node& node = nodes_[id];
        if (!node.bounds.overlaps(bounds)) {
            continue;
        }
        if (node.children) {
            stack.push_back(node.children->second);
            stack.push_back(node.children->first);
        } else {
            for (std::size_t k = node.range.start; k < node.range.end; ++k) {
                const std::size_t i = order_[k];
                if (meshes[i].local_bounds.overlaps(bounds)) result.push_back(i);
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

}  // namespace s3::physics::board_world::query_index
