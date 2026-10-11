// Port of skate-game/src/physics/offboard/ground_sync.rs
#include "s3/game/physics/offboard/ground_sync.hpp"

namespace s3::game::physics::offboard::ground_sync {

rs::Result<std::vector<Edge>, const char*> SceneService::edge_candidates(const EdgeSearch& search) {
    // Dynamic providers are host owned and are deliberately empty in the
    // current single-player level. Static authored edges remain enumerated
    // by with_world_scene from the world's query metadata.
    return ground_query::with_world_scene<std::vector<Edge>>(
        *world, ground_query::PrimaryEdges::Normal{}, sc::physics::Slice<const ground_query::IndexedEdgeBody>{},
        [&](ground_query::world::WorldScene& scene) { return scene.edge_candidates(search); });
}

rs::Result<std::array<std::optional<LineHit>, 7>, const char*> SceneService::query_lines(
    const GroundQueryPacket& packet) {
    return ground_query::with_world_scene<std::array<std::optional<LineHit>, 7>>(
        *world, ground_query::PrimaryEdges::Normal{}, sc::physics::Slice<const ground_query::IndexedEdgeBody>{},
        [&](ground_query::world::WorldScene& scene) { return scene.query_lines(packet); });
}

}  // namespace s3::game::physics::offboard::ground_sync
