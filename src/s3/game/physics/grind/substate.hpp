// Port of skate-game/src/physics/grind/substate.rs.
//! Original82D40AF8: collision -> pop -> engagement -> derived substate.
//! All writes target current board, manager jumper and skeleton owners.
#pragma once
#include <array>
#include <optional>
#include <string>

#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"
#include "s3/game/physics/grind/observation.hpp"
#include "s3/game/physics/grind/substate/nonspecific.hpp"
#include "s3/game/physics/grind/substate/settings.hpp"
#include "s3/game/physics/grind/substate/skeleton.hpp"

namespace s3::game::physics::grind::substate {

using observation::ManagerObservation;
/// `pub(super) use nonspecific::execute as execute_nonspecific;`
inline Status execute_nonspecific(GamePhysics& physics, SkaterRuntime& skater) {
    return nonspecific::execute(physics, skater);
}
using settings::Settings;

Status execute(GamePhysics& physics, SkaterRuntime& skater, const ManagerObservation& manager);

///Shared82D944E8 caller for common grind and Nonspecific. The late-false
///output is intentionally ignored, and angular displacement is not applied.
///Returns prediction velocity ONLY on the branch that queued force15.
rs::Result<std::optional<std::array<float, 4>>, std::string> collision_force(GamePhysics& physics,
                                                                             const SkaterRuntime& skater);

}  // namespace s3::game::physics::grind::substate
