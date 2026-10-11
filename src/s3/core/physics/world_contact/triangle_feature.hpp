// Port of skate-core/src/physics/world_contact/triangle_feature.rs.
#pragma once
#include "s3/core/physics/world_contact/arithmetic.hpp"

namespace s3::physics::world_contact::triangle_feature {

/// Full triangle maximum-feature callback82ADDD68, including near-tangent
/// edge/vertex branches. Header word0 is preserved outside the face branch.
void triangle_maximum_feature(const std::array<std::uint32_t, 48>& gp, std::uint32_t mode,
                              std::array<std::uint32_t, 4> direction, MaximumFeature& out);

/// Complete82AC6F88. Only edge plane vectors are changed. Mode0 negates the
/// incoming direction before crossing, preserving the native arithmetic order.
void build_feature_edge_planes(MaximumFeature& feature, std::uint32_t mode, std::array<std::uint32_t, 4> direction);

}  // namespace s3::physics::world_contact::triangle_feature
