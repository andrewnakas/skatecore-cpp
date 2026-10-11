// Port of skate-core/src/physics/world_contact/dispatch.rs.
#pragma once
#include "s3/core/physics/world_contact/arithmetic.hpp"
#include "s3/core/physics/world_contact/face_face.hpp"
#include "s3/core/physics/world_contact/face_specialized.hpp"

namespace s3::physics::world_contact::dispatch {

/// TU3 82ACE190, FindFeatureIntersectionPrism. Features contain a point (zero
/// edges), segment (one), or convex face. Both feature tags may be updated by
/// closest-feature selection. The caller owns the initial normal/override flag;
/// they change only when the recovered helpers explicitly correct the normal.
///
/// Numerical helpers retain the core's current host arithmetic approximation;
/// this source port has not been compared with a Skate 3 hardware capture.
std::uint32_t find_feature_intersection_prism(FeaturePrism& output, MaximumFeature& a, MaximumFeature& b,
                                              std::array<std::uint32_t, 4> normal);

}  // namespace s3::physics::world_contact::dispatch
