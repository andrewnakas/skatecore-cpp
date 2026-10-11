// Port of skate-game/src/physics/offboard/ground_query/world.rs
//! Ephemeral adapter over the canonical BoardWorld metadata. Geometry is borrowed.
#pragma once
#include <array>
#include <optional>
#include <utility>
#include <vector>

#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/game/physics/offboard/ground_query.hpp"

namespace s3::game::physics::offboard::ground_query::world {

/// Static mesh candidates use the same ZIP hierarchy as the other world queries.
/// The existing ground leaf still owns transforms, exact bounds gates and hits.
struct WorldScene : GroundQueryScene<const char*> {
    // Private in Rust.
    const BoardWorld* world = nullptr;
    Scene edges;

    WorldScene(const BoardWorld* world_, Scene edges_) : world(world_), edges(edges_) {}

    /// `impl GroundQueryScene for WorldScene`.
    rs::Result<std::vector<Edge>, const char*> edge_candidates(const EdgeSearch& search) override;
    rs::Result<std::array<std::optional<LineHit>, 7>, const char*> query_lines(
        const GroundQueryPacket& packet) override;
};

/// private
Frame frame(const sc::physics::drive_frames::RetailAffineTransform& f);

/// Caller supplies actual dynamic-provider views from the same live scene owner.
/// Empty providers mean the authored scene actually has none, never a failed lookup.
/// `operation: impl FnOnce(&mut WorldScene) -> Result<T, &'static str>`.
template <class T, class F>
rs::Result<T, const char*> with_world_scene(const BoardWorld& world, PrimaryEdges primary_edges,
                                            Slice<const IndexedEdgeBody> indexed_edges, F&& operation) {
    auto metadata_ = world.query_metadata();
    if (metadata_.is_err()) return rs::make_err(metadata_.unwrap_err());
    const auto* metadata = metadata_.unwrap();
    std::vector<Segment> edges;
    edges.reserve(metadata->static_edges.size());
    for (const auto& s : metadata->static_edges) {
        edges.push_back(Segment{Edge{s.start, s.end}, Bounds{s.local_bounds.min, s.local_bounds.max}});
    }
    Scene edge_scene(Slice<const Mesh>{}, Slice<const Mesh>{}, Slice<const Mesh>{}, metadata->island_flags,
                     Slice<const Segment>(edges), primary_edges, indexed_edges);
    WorldScene scene(&world, edge_scene);
    return std::forward<F>(operation)(scene);
}

}  // namespace s3::game::physics::offboard::ground_query::world
