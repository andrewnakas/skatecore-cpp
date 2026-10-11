// Port of skate-game/src/grind_world/derive.rs (s3 version; src/derive.cpp is the separate "lite" engine's copy).
//! Grind edges derived from triangle collision, for custom maps only. Stock
//! maps carry their own splines and never reach this. An edge qualifies when
//! it is a convex crease on a rideable face with a real drop beside it.
#pragma once
#include <array>
#include <vector>

namespace s3::game::grind_world::derive {

struct Derived {
    /// Parallel to the map's own rails: true where the engine could never
    /// grind it, because the surface under it is wider than a rail.
    std::vector<bool> own_blocked;
    std::vector<std::vector<std::array<float, 3>>> rails;
};

/// Polylines for every derived grind edge. `own` are the map's own rails;
/// edges that already have a usable one are left out.
Derived rails(const std::vector<std::array<std::array<float, 3>, 3>>& collision,
              const std::vector<std::vector<std::array<float, 3>>>& own);

}  // namespace s3::game::grind_world::derive
