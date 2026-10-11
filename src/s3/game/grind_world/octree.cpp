// Port of skate-game/src/grind_world/octree.rs.
//! Original S3 loose octree insertion/query order, 82AD6830/82476158.
//! Host-owned nodes retain native head insertion, split redistribution and
//! LIFO candidate traversal because result capacity is observably bounded.
#include "s3/game/grind_world/octree.hpp"

namespace s3::game::grind_world::octree {

Bounds Bounds::identity_transformed() const {
    std::array<float, 3> center{};
    std::array<float, 3> half{};
    for (std::size_t i = 0; i < 3; ++i) center[i] = (max[i] + min[i]) * 0.5f;
    for (std::size_t i = 0; i < 3; ++i) half[i] = (max[i] - min[i]) * 0.5f;
    Bounds out;
    for (std::size_t i = 0; i < 3; ++i) out.min[i] = center[i] - half[i];
    for (std::size_t i = 0; i < 3; ++i) out.max[i] = center[i] + half[i];
    return out;
}

bool Bounds::overlaps(Bounds b) const {
    for (std::size_t i = 0; i < 3; ++i) {
        if (!(min[i] <= b.max[i] && b.min[i] <= max[i])) return false;
    }
    return true;
}

Bounds Bounds::union_(Bounds b) const {
    Bounds out;
    for (std::size_t i = 0; i < 3; ++i) out.min[i] = rs::min(min[i], b.min[i]);
    for (std::size_t i = 0; i < 3; ++i) out.max[i] = rs::max(max[i], b.max[i]);
    return out;
}

Bounds Bounds::padded() const {
    Bounds out;
    for (std::size_t i = 0; i < 3; ++i) out.min[i] = min[i] - 1.0f;
    for (std::size_t i = 0; i < 3; ++i) out.max[i] = max[i] + 1.0f;
    return out;
}

Bounds Bounds::cube() const {
    std::array<float, 3> half{};
    for (std::size_t i = 0; i < 3; ++i) half[i] = (max[i] - min[i]) * 0.5f;
    const float radius = rs::max(rs::max(half[0], half[1]), half[2]);
    std::array<float, 3> center{};
    for (std::size_t i = 0; i < 3; ++i) center[i] = (max[i] + min[i]) * 0.5f;
    Bounds out;
    for (std::size_t i = 0; i < 3; ++i) out.min[i] = center[i] - radius;
    for (std::size_t i = 0; i < 3; ++i) out.max[i] = center[i] + radius;
    return out;
}

// 82F838B0 loads 82098E30 = BECC_CCCD into 830BDD20: -0.4.
Bounds Bounds::inner() const {
    std::array<float, 3> padding{};
    for (std::size_t i = 0; i < 3; ++i) padding[i] = (max[i] - min[i]) * rs::from_bits(0xbecccccdu);
    Bounds out;
    for (std::size_t i = 0; i < 3; ++i) out.min[i] = min[i] - padding[i];
    for (std::size_t i = 0; i < 3; ++i) out.max[i] = max[i] + padding[i];
    return out;
}

Bounds Bounds::child(std::size_t index) const {
    const Bounds in = inner();
    Bounds out;
    for (std::size_t i = 0; i < 3; ++i) out.min[i] = (index & (std::size_t(1) << i)) != 0 ? in.min[i] : min[i];
    for (std::size_t i = 0; i < 3; ++i) out.max[i] = (index & (std::size_t(1) << i)) != 0 ? max[i] : in.max[i];
    return out;
}

std::optional<std::pair<std::size_t, Bounds>> Bounds::containing_child(Bounds entry) const {
    const Bounds in = inner();
    std::size_t index = 0;
    for (std::size_t i = 0; i < 3; ++i) {
        const float low_margin = entry.min[i] - in.min[i];
        const float high_margin = in.max[i] - entry.max[i];
        if (low_margin <= 0.0f && high_margin <= 0.0f) {
            return std::nullopt;
        }
        if (low_margin > high_margin) {
            index |= std::size_t(1) << i;
        }
    }
    return std::make_pair(index, child(index));
}

bool Bounds::movable(Bounds entry) const {
    const Bounds in = inner();
    for (std::size_t i = 0; i < 3; ++i) {
        if (!(in.max[i] > entry.max[i] || entry.min[i] > in.min[i])) return false;
    }
    return true;
}

Node Node::new_(Bounds bounds) {
    Node n;
    n.bounds = bounds;
    return n;
}

rs::Result<Octree, std::string> Octree::new_(Bounds asset_bounds, std::vector<Bounds> bounds) {
    // Native entries and node links are uint16; 0xffff is the sentinel.
    if (bounds.size() > 0xffff) {
        return rs::make_err(std::string("Native grind octree exceeds uint16 entry capacity"));
    }
    Octree tree;
    tree.node_capacity = bounds.size() / 2 + 1;
    tree.nodes.push_back(Node::new_(asset_bounds.cube()));
    tree.bounds = std::move(bounds);
    for (std::size_t index = 0; index < tree.bounds.size(); ++index) {
        tree.insert(index);
    }
    return tree;
}

void Octree::insert(std::size_t entry) {
    std::size_t at = 0;
    for (;;) {
        const Bounds node_bounds = nodes[at].bounds;
        bool inside = true;
        for (std::size_t i = 0; i < 3; ++i) {
            if (!(bounds[entry].min[i] >= node_bounds.min[i] && bounds[entry].max[i] <= node_bounds.max[i])) {
                inside = false;
                break;
            }
        }
        const auto target = inside ? node_bounds.containing_child(bounds[entry]) : std::nullopt;
        if (!target) {
            nodes[at].resident.push_back(entry);
            return;
        }
        const std::size_t slot = target->first;
        const Bounds child_bounds = target->second;
        if (const auto child = nodes[at].buckets[slot].child) {
            at = *child;
            continue;
        }
        const bool movable = child_bounds.movable(bounds[entry]);
        Bucket& bucket = nodes[at].buckets[slot];
        bucket.entries.emplace_back(entry, movable);
        std::size_t movable_count = 0;
        for (const auto& e : bucket.entries) {
            if (e.second) ++movable_count;
        }
        if (movable_count > 3 && nodes.size() < node_capacity) {
            split(at, slot, child_bounds);
        }
        return;
    }
}

void Octree::split(std::size_t parent, std::size_t slot, Bounds node_bounds) {
    std::vector<std::pair<std::size_t, bool>> entries = std::move(nodes[parent].buckets[slot].entries);
    nodes[parent].buckets[slot].entries.clear();
    const std::size_t child = nodes.size();
    nodes[parent].buckets[slot].child = child;
    Node node = Node::new_(node_bounds);
    for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
        const std::size_t entry = it->first;
        if (it->second) {
            const auto next = node_bounds.containing_child(bounds[entry]);
            if (!next) S3_PANIC("native movable predicate guarantees a containing child");
            node.buckets[next->first].entries.emplace_back(entry, next->second.movable(bounds[entry]));
        } else {
            node.resident.push_back(entry);
        }
    }
    nodes.push_back(std::move(node));
}

std::vector<std::size_t> Octree::query(Bounds query, std::size_t limit) const {
    std::vector<std::size_t> result;
    if (limit == 0) {
        return result;
    }
    std::vector<std::size_t> stack = {0};
    while (!stack.empty()) {
        const std::size_t index = stack.back();
        stack.pop_back();
        const Node& node = nodes[index];
        // Buckets retain movable bits; traverse directly in the same order
        // instead of manufacturing a second copy of every entry list.
        std::vector<std::size_t> leaf_slots;
        for (std::size_t slot = 0; slot < 8; ++slot) {
            if (node.bounds.child(slot).overlaps(query)) {
                if (const auto child = node.buckets[slot].child) {
                    stack.push_back(*child);
                } else {
                    leaf_slots.push_back(slot);
                }
            }
        }
        for (auto s = leaf_slots.rbegin(); s != leaf_slots.rend(); ++s) {
            const auto& entries = node.buckets[*s].entries;
            for (auto e = entries.rbegin(); e != entries.rend(); ++e) {
                if (bounds[e->first].overlaps(query)) {
                    result.push_back(e->first);
                    if (result.size() == limit) {
                        return result;
                    }
                }
            }
        }
        for (auto r = node.resident.rbegin(); r != node.resident.rend(); ++r) {
            if (bounds[*r].overlaps(query)) {
                result.push_back(*r);
                if (result.size() == limit) {
                    return result;
                }
            }
        }
    }
    return result;
}

}  // namespace s3::game::grind_world::octree
