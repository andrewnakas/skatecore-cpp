// Port of skate-game/src/grind_world/octree.rs.
//! Original S3 loose octree insertion/query order, 82AD6830/82476158.
//! Host-owned nodes retain native head insertion, split redistribution and
//! LIFO candidate traversal because result capacity is observably bounded.
#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "s3/core/rs.hpp"

namespace s3::game::grind_world::octree {

namespace rs = ::s3::rs;

struct Bounds {
    std::array<float, 3> min{};
    std::array<float, 3> max{};

    /// 8258C1E0 still executes center/half-extent arithmetic when the supplied
    /// transform is identity. Do not optimize it to copying the authored box.
    Bounds identity_transformed() const;
    bool overlaps(Bounds b) const;
    Bounds union_(Bounds b) const;  // `union`
    Bounds padded() const;
    // Private in Rust.
    Bounds cube() const;
    // 82F838B0 loads 82098E30 = BECC_CCCD into 830BDD20: -0.4.
    Bounds inner() const;
    Bounds child(std::size_t index) const;
    /// 82AD6450: select the side with greater containment margin, ties low.
    std::optional<std::pair<std::size_t, Bounds>> containing_child(Bounds entry) const;
    /// 82AD6530: could this entry fit another level? Strict comparisons.
    bool movable(Bounds entry) const;
};
S3_DERIVE_EQ(Bounds, min, max)

struct Bucket {
    std::vector<std::pair<std::size_t, bool>> entries;
    std::optional<std::size_t> child;
};

struct Node {
    Bounds bounds;
    std::vector<std::size_t> resident;
    std::array<Bucket, 8> buckets{};

    static Node new_(Bounds bounds);
};

struct Octree {
    // Private in Rust.
    std::vector<Node> nodes;
    std::vector<Bounds> bounds;
    std::size_t node_capacity = 0;

    static rs::Result<Octree, std::string> new_(Bounds asset_bounds, std::vector<Bounds> bounds);
    /// 82AD6D18 pushes resident then ascending leaf lists; 82476158 consumes
    /// those lists LIFO before taking descending deferred child nodes.
    std::vector<std::size_t> query(Bounds query, std::size_t limit) const;
    // Private in Rust.
    void insert(std::size_t entry);
    /// 82AD65D8: walk old head-first list and prepend each redistributed entry;
    /// no recursive split occurs during this redistribution.
    void split(std::size_t parent, std::size_t slot, Bounds bounds);
};

}  // namespace s3::game::grind_world::octree
