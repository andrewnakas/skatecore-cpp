// Port of skate-core/src/physics/world_contact/box_box_sat.rs
// (module physics::world_contact::primitive_query::box_box_sat via #[path]).
#pragma once
#include <utility>

#include "s3/core/physics/world_contact/arithmetic.hpp"

namespace s3::physics::world_contact::primitive_query::box_box_sat {

/// TU3 82ACF448. Unlike the triangle/box entry, edge crosses use the raw
/// reciprocal-square-root estimate without refinement. The last cross is
/// repeated to fill its final four-candidate batch; equal scores retain the
/// earlier axis. Shape fatness is excluded from these intervals.
std::pair<std::array<std::uint32_t, 4>, std::array<std::uint32_t, 4>> box_box(const std::array<std::uint32_t, 48>& a,
                                                                              const std::array<std::uint32_t, 48>& b);

}  // namespace s3::physics::world_contact::primitive_query::box_box_sat
