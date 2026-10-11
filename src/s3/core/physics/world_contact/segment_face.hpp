// Port of skate-core/src/physics/world_contact/segment_face.rs.
#pragma once
#include "s3/core/physics/world_contact/arithmetic.hpp"

namespace s3::physics::world_contact::segment_face {

/// Complete TU3 82ACC5F0 segment/face prism and its interval/edge-clamp callees.
/// Keeps the normal correction flag, feature tags and both output-pointer orders.
std::uint32_t intersect_segment_face(FeaturePrism& output, MaximumFeature& face, MaximumFeature& segment,
                                     std::array<std::uint32_t, 4> normal_bits, bool face_is_a);

}  // namespace s3::physics::world_contact::segment_face
