// Port of skate-core/src/physics/world_contact/box_feature.rs.
#pragma once
#include "s3/core/physics/world_contact/arithmetic.hpp"

namespace s3::physics::world_contact::box_feature {

/// Complete native box maximum-feature callback82AD7A98. Face winding uses the
/// full integer mode. `incoming_edge_plane` is VMX v60 at entry, copied only in
/// the edge branch without initialization by the native function.
void box_maximum_feature(const std::array<std::uint32_t, 48>& gp, std::uint32_t mode,
                         std::array<std::uint32_t, 4> direction, MaximumFeature& output,
                         std::array<std::uint32_t, 4> incoming_edge_plane);

}  // namespace s3::physics::world_contact::box_feature
