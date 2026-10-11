// Port of skate-game/src/physics/plant_skeleton.rs.
//! Skeleton82BDEC48 (anchored foot) and82BDF090 (plant COM).
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::plant_skeleton {

/// `pub(super)`.
Status advance(GamePhysics& physics, SkaterRuntime& skater, std::array<float, 4> anchor,
               std::optional<std::size_t> bone);
/// `pub(super)`.
void hold_foot(SkaterRuntime& skater, bool right, std::array<float, 4> position, std::uint32_t frames);

}  // namespace s3::game::physics::plant_skeleton
