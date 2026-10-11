// Port of skate-game/src/physics/ground.rs.
//! Our level geometry, queried with the recovered physics/world calculations.
#pragma once
#include <array>
#include <utility>
#include <vector>

#include "s3/core/math.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/collision.hpp"
#include "s3/core/physics/contact.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::ground {

using sc::math::Vector3;
using sc::physics::board_world::BoardWorld;
using sc::physics::board_world::query_metadata::EdgeSegment;
using sc::physics::contact::RetailContactMaterial;
using Quad = std::array<Vector3, 4>;
using Surfaces = std::array<std::vector<Quad>, 4>;

inline constexpr float HEIGHT = -0.035f;

/// One authored terrain selection drives both presentation and live queries.
/// `#[derive(Clone, Copy, Debug, PartialEq, Eq)]`
enum class Terrain {
    Flat,
    Course,
};
/// `impl Terrain { fn surfaces(self) }`
Surfaces surfaces(Terrain self);
/// `impl Terrain { fn world(self, material) }`
BoardWorld world(Terrain self, RetailContactMaterial material);

/// Authored level dimensions in metres; HEIGHT remains the starting surface.
inline constexpr float FLOOR_HEIGHT = HEIGHT - 1.5f;

/// Shared quads for rendering and collision: landing floor, starting box, half pipe.
/// Quads use the same winding as the original floor (0,2,1 and 0,3,2).
Surfaces surfaces();

/// `pub(super)`.
BoardWorld world(RetailContactMaterial material);

/// Deliberately authored flat-ground comparison surface. Both the solver and
/// all gameplay probes query these real collision triangles.
Surfaces flat_surfaces();
BoardWorld flat_world(RetailContactMaterial material);

/// Private in Rust.
BoardWorld build_world(RetailContactMaterial material, Surfaces surfaces, std::vector<EdgeSegment> edges);
///Deliberately authored ledge records, in level order. Neither the triangle
///diagonals nor the smooth ramp/half-pipe tessellation seams are ledges.
std::vector<EdgeSegment> query_edges();

/// `pub(super)`.
std::pair<sc::physics::collision::WorldContactSettings, sc::physics::board_world::ContactRetentionSettings>
query_settings();

}  // namespace s3::game::physics::ground
