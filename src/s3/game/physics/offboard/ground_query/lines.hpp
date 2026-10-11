// Port of skate-game/src/physics/offboard/ground_query/lines.rs
#pragma once
#include <array>
#include <optional>

#include "s3/game/physics/offboard/ground_query.hpp"

namespace s3::game::physics::offboard::ground_query::lines {

/// pub(super)
rs::Result<std::array<std::optional<LineHit>, 7>, const char*> query(const Scene& scene,
                                                                     const GroundQueryPacket& packet);
/// pub(super)
rs::Result<std::array<std::optional<LineHit>, 7>, const char*> query_pools(const std::array<Slice<const Mesh>, 3>& pools,
                                                                           const GroundQueryPacket& packet);

}  // namespace s3::game::physics::offboard::ground_query::lines
