// Port of skate-game/src/physics/air_phase/skeleton.rs.
//! Same live Skeleton owners as Ground; Air chooses its original update method.
#pragma once
#include <array>
#include <optional>

#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::air_phase::skeleton {

Status advance(GamePhysics& physics, SkaterRuntime& skater, std::optional<std::array<float, 4>> com);

}  // namespace s3::game::physics::air_phase::skeleton
