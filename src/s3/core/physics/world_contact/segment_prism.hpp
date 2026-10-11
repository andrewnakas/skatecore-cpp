// Port of skate-core/src/physics/world_contact/segment_prism.rs.
#pragma once
#include "s3/core/physics/world_contact/arithmetic.hpp"

namespace s3::physics::world_contact::segment_prism {

/// Complete TU3 82ACBAD0 segment/segment prism, including the parallel overlap
/// and disjoint endpoint branches. Normal/flags and untouched point slots survive.
/// `a_is_first` represents the helper's two independently supplied output pointers.
std::uint32_t intersect_feature_segments(FeaturePrism& output, MaximumFeature& a, MaximumFeature& b,
                                         std::array<std::uint32_t, 4> normal, bool a_is_first);

}  // namespace s3::physics::world_contact::segment_prism
