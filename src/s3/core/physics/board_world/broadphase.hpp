// Port of skate-core/src/physics/board_world/broadphase.rs (free functions; the
// `impl BoardWorld` methods are declared in board_world.hpp).
// ZIP world broadphase helpers; recovered narrow-phase stays in its existing modules.
#pragma once
#include <optional>

#include "s3/core/physics/board_world.hpp"

namespace s3::physics::board_world::broadphase {

// Positive magnitude of thin_triangle's existing 0xb727_c5ac tolerance.
inline constexpr float THIN_MARGIN = rs::from_bits(0x3727c5acu);

/// pub(super)
std::optional<Bounds> primitive_bounds(const ContactPrimitive& primitive);

/// pub(super)
Bounds conservative_bounds(Bounds bounds, float padding);

}  // namespace s3::physics::board_world::broadphase
