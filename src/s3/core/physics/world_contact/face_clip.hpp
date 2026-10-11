// Port of skate-core/src/physics/world_contact/face_clip.rs.
#pragma once
#include "s3/core/physics/world_contact/arithmetic.hpp"

namespace s3::physics::world_contact::face_clip {

/// Complete TU3 82ACC298. Projects an exterior point onto the selected edge;
/// returns its edge/region code without modifying the feature header itself.
std::uint32_t clamp_point_to_feature(const MaximumFeature& face, std::array<std::uint32_t, 4> normal,
                                     std::array<std::uint32_t, 4>& point);

/// Complete TU3 82ACC400 interval clipping. `interval` stores the two scalar
/// limits; `outside` stores the last rejecting edge origin and normalized plane.
/// Those vectors are preserved unless a near-parallel exterior edge writes them.
std::uint32_t clip_segment_to_feature(const MaximumFeature& face, const MaximumFeature& segment,
                                      std::array<std::uint32_t, 4> normal, std::array<std::uint32_t, 2>& interval,
                                      std::array<std::uint32_t, 8>& outside);

}  // namespace s3::physics::world_contact::face_clip
