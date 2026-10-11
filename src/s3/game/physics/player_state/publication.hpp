// Port of skate-game/src/physics/player_state/publication.rs.
#pragma once
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::player_state::publication {

/// `pub(super)`.
Status publish(GamePhysics& physics, SkaterRuntime& skater);

}  // namespace s3::game::physics::player_state::publication
