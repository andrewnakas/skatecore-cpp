// Port of skate-core/src/physics/world_contact/triangle_box_sat.rs
// (module physics::world_contact::primitive_query::triangle_box_sat via #[path]).
#pragma once
#include <utility>

#include "s3/core/physics/world_contact/arithmetic.hpp"

namespace s3::physics::world_contact::primitive_query::triangle_box_sat {

/// TU3 82ACF950. Triangle normal first, then box Z/Y/X and all nine edge
/// crosses. Cross axes receive exactly one reciprocal-square-root refinement;
/// there is no small-cross rejection or normalization fallback in this entry.
std::pair<std::array<std::uint32_t, 4>, std::array<std::uint32_t, 4>> triangle_box(
    const std::array<std::uint32_t, 48>& triangle, const std::array<std::uint32_t, 48>& box_shape);

}  // namespace s3::physics::world_contact::primitive_query::triangle_box_sat
