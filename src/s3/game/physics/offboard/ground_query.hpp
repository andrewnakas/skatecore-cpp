// Port of skate-game/src/physics/offboard/ground_query.rs
//! Borrowed scene adapter for original Biped query82764AB0/82770650/82C1EAD8.
//! All geometry and live transforms belong to the canonical collision scene.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "s3/core/math.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/slice.hpp"
#include "s3/core/player/offboard/ground_query.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::offboard::ground_query {

using sc::math::Vector3;
using sc::physics::Slice;
using sc::physics::board_world::BoardWorld;
using sc::physics::board_world::WorldTriangle;
using sc::player::offboard::ground_query::Edge;
using sc::player::offboard::ground_query::EdgeSearch;
using sc::player::offboard::ground_query::Frame;
using sc::player::offboard::ground_query::GroundQueryPacket;
using sc::player::offboard::ground_query::GroundQueryScene;
using sc::player::offboard::ground_query::LineHit;

/// #[derive(Clone, Copy, Debug)]
struct Bounds {
    Vector3 min{};
    Vector3 max{};
};
/// A borrowed mesh from one canonical source pool, in canonical mesh order.
struct Mesh {
    Frame local_to_world{};
    Frame world_to_local{};
    Bounds local_bounds{};
    std::int32_t matching_group = 0;
    Slice<const WorldTriangle> triangles{};
    /// Exact packed source surface, one entry per triangle; never inferred from tag.
    Slice<const std::uint16_t> surfaces{};
};
/// #[derive(Clone, Copy)]
struct Segment {
    Edge edge{};
    Bounds local_bounds{};
};
struct EdgeBody {
    Frame local_to_world{};
    Bounds local_bounds{};
    Slice<const Segment> segments{};
};
struct AlternateRecord {
    bool enabled = false;
    /// Original record44 and48, selected by context2948 bit1.
    std::array<std::optional<std::pair<const EdgeBody*, std::int32_t>>, 2> choices{};
};
/// Rust `enum PrimaryEdges<'a>`.
struct PrimaryEdges {
    /// Original826C0630: dynamic provider, then vehicle provider.
    struct Normal {
        Slice<const EdgeBody> dynamic{};
        Slice<const EdgeBody> vehicles{};
    };
    struct Alternate {
        Slice<const AlternateRecord> records{};
    };
    std::variant<Normal, Alternate> value;

    PrimaryEdges(Normal v) : value(v) {}
    PrimaryEdges(Alternate v) : value(v) {}
};
struct IndexedEdgeBody {
    std::uint32_t id = 0;
    bool disabled = false;
    const EdgeBody* body = nullptr;
};
/// Ephemeral view only: owns no collision geometry, bodies, or cached observations.
struct Scene : GroundQueryScene<const char*> {
    Slice<const Mesh> ground_pool{};
    Slice<const Mesh> island_pool{};
    Slice<const Mesh> conditional_pool{};
    std::uint32_t island_flags = 0;
    Slice<const Segment> static_edges{};
    PrimaryEdges primary_edges{PrimaryEdges::Normal{}};
    Slice<const IndexedEdgeBody> indexed_edges{};

    Scene(Slice<const Mesh> ground_pool_, Slice<const Mesh> island_pool_, Slice<const Mesh> conditional_pool_,
          std::uint32_t island_flags_, Slice<const Segment> static_edges_, PrimaryEdges primary_edges_,
          Slice<const IndexedEdgeBody> indexed_edges_)
        : ground_pool(ground_pool_),
          island_pool(island_pool_),
          conditional_pool(conditional_pool_),
          island_flags(island_flags_),
          static_edges(static_edges_),
          primary_edges(primary_edges_),
          indexed_edges(indexed_edges_) {}

    /// `impl GroundQueryScene for Scene`.
    rs::Result<std::vector<Edge>, const char*> edge_candidates(const EdgeSearch& search) override;
    rs::Result<std::array<std::optional<LineHit>, 7>, const char*> query_lines(
        const GroundQueryPacket& packet) override;
};

namespace world {
struct WorldScene;
template <class T, class F>
rs::Result<T, const char*> with_world_scene(const BoardWorld& world, PrimaryEdges primary_edges,
                                            Slice<const IndexedEdgeBody> indexed_edges, F&& operation);
}  // namespace world
using world::with_world_scene;

}  // namespace s3::game::physics::offboard::ground_query

#include "s3/game/physics/offboard/ground_query/edges.hpp"
#include "s3/game/physics/offboard/ground_query/lines.hpp"
#include "s3/game/physics/offboard/ground_query/transform.hpp"
#include "s3/game/physics/offboard/ground_query/world.hpp"
