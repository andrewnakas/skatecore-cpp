// Port of skate-game/src/physics/offboard/ground_sync.rs
//! Host side scene service for the recovered BipedGround synchronizer.
//!
//! The synchronizer owns the query ordering in `skate-core`; this adapter only
//! exposes the already loaded BoardWorld.  It does not cache contacts or run a
//! second solver.  Every query therefore sees the same authored triangles and
//! packed surfaces as the board collision pass.
#pragma once
#include <array>
#include <optional>
#include <vector>

#include "s3/core/physics/board_world.hpp"
#include "s3/core/player/offboard/ground_query.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/offboard/ground_query.hpp"

namespace s3::game::physics::offboard::ground_sync {

using sc::physics::board_world::BoardWorld;
using sc::player::offboard::ground_query::Edge;
using sc::player::offboard::ground_query::EdgeSearch;
using sc::player::offboard::ground_query::GroundQueryPacket;
using sc::player::offboard::ground_query::GroundQueryScene;
using sc::player::offboard::ground_query::LineHit;

struct SceneService : GroundQueryScene<const char*> {
    const BoardWorld* world = nullptr;

    explicit SceneService(const BoardWorld* world_) : world(world_) {}

    /// `impl GroundQueryScene for SceneService`.
    rs::Result<std::vector<Edge>, const char*> edge_candidates(const EdgeSearch& search) override;
    rs::Result<std::array<std::optional<LineHit>, 7>, const char*> query_lines(
        const GroundQueryPacket& packet) override;
};

}  // namespace s3::game::physics::offboard::ground_sync
