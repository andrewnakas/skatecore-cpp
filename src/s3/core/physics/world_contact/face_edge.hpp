// Port of skate-core/src/physics/world_contact/face_edge.rs.
#pragma once
#include "s3/core/physics/world_contact/arithmetic.hpp"

namespace s3::physics::world_contact::face_edge {

/// Complete TU3 82ACCA28 corner/edge fallback. Returns one or two point pairs;
/// the caller writes the prism count. Both feature headers are replaced here.
std::uint32_t intersect_feature_corner_edge(FeaturePrism& output, MaximumFeature& a, MaximumFeature& b,
                                            std::size_t corner, std::size_t edge, bool a_is_first);

}  // namespace s3::physics::world_contact::face_edge
