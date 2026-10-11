// Port of skate-game/src/physics/footplant/ground.rs.
//! FootPlant601, vtable82327400 and manager82D704B0/82D70E40.
//! (`impl Footplant` start/adjust are declared in footplant.hpp.)
#pragma once
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::footplant::ground {

/// `pub(in crate::physics)`.
Status enter(GamePhysics& physics, SkaterRuntime& skater);
Status update(GamePhysics& physics, SkaterRuntime& skater);
///82D4C6D8, after the shared plant collision check82D8FDC0.
void post_physics(SkaterRuntime& skater);

}  // namespace s3::game::physics::footplant::ground
