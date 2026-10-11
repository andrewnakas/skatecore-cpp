// Port of skate-game/src/physics/offboard/contact_toolkit/world.rs
//!82770B40/82771D08 mesh traversal and82770E00/82772028 nearby collection.
//!This adapter only accepts BoardWorld's authored static-world contract.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "s3/core/air/trajectory.hpp"
#include "s3/core/math.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/board_world/query_metadata.hpp"
#include "s3/core/player/offboard/contact_toolkit.hpp"
#include "s3/core/player/offboard/ground_query.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::offboard::contact_toolkit::world {

using sc::air::trajectory::QueryRequest;
using sc::air::trajectory::QueryResult;
using sc::physics::board_world::BoardWorld;
using sc::physics::board_world::query_metadata::QueryMetadata;
using sc::physics::board_world::query_metadata::QueryPool;
using sc::player::offboard::contact_toolkit::Batch;
using sc::player::offboard::contact_toolkit::LineHit;
using sc::player::offboard::contact_toolkit::LineProbe;
using sc::player::offboard::contact_toolkit::QueryResults;
using sc::player::offboard::contact_toolkit::Vector;

struct StaticScene : sc::player::offboard::contact_toolkit::Scene<const char*> {
    // Private in Rust.
    const BoardWorld* world = nullptr;
    const QueryMetadata* metadata = nullptr;

    StaticScene(const BoardWorld* world_, const QueryMetadata* metadata_) : world(world_), metadata(metadata_) {}

    ///82C20728/82764AB0: caller-sized lines, poolmask7, facing3, no mesh or
    ///material exclusion. In particular Air submits SIX lines, Ground seven.
    rs::Result<std::vector<std::optional<sc::player::offboard::ground_query::LineHit>>, const char*> lines(
        const std::vector<sc::player::offboard::ground_query::Line>& requests, std::int32_t matching_group) const;
    /// 82764CF8 pool mask 7; 82770910 walks the actual accelerated trajectory.
    /// 8276EE88 retains the first pool on equal contact times. Callers supply
    /// the actor matching group and the original per-request mesh rejection mask.
    rs::Result<QueryResult, const char*> trajectory(QueryRequest request, std::int32_t matching_group,
                                                    std::uint32_t mesh_reject_mask) const;
    static rs::Result<StaticScene, const char*> new_(const BoardWorld& world);

    // ---- private in Rust ----
    /// `impl Iterator<Item = QueryPool>`.
    std::vector<QueryPool> pools() const;
    rs::Result<std::optional<LineHit>, const char*> line(QueryPool pool, LineProbe probe, std::int32_t group,
                                                         std::uint32_t reject) const;
    rs::Result<std::vector<std::array<Vector, 3>>, const char*> nearby(QueryPool pool, Vector center, float radius,
                                                                       std::int32_t group) const;
    /// 82D81610 bounds and 82C1EAD8 ordinary static-edge traversal/capacity.
    std::vector<std::array<Vector, 2>> edges(const Batch& batch) const;

    /// `impl Scene for StaticScene`.
    rs::Result<QueryResults, const char*> execute(const Batch& batch) const override;
};

}  // namespace s3::game::physics::offboard::contact_toolkit::world
