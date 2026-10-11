// Port of skate-core/src/physics/world_contact/point_feature.rs.
#pragma once
#include "s3/core/physics/world_contact/arithmetic.hpp"

namespace s3::physics::world_contact::point_feature {

/// Complete TU3 82AC6EE8. Region codes are before=1, interior=2, after=3.
/// The upper-end comparison tests all four length lanes; equality is interior.
/// (`segment` points at 16 words, `point` at 4 words.)
std::uint32_t closest_feature_segment(const std::uint32_t* segment, std::uint32_t* point);
inline std::uint32_t closest_feature_segment(const std::array<std::uint32_t, 16>& segment,
                                             std::array<std::uint32_t, 4>& point) {
    return closest_feature_segment(segment.data(), point.data());
}

/// Complete TU3 82ACC160 point/face helper. `face_is_a` selects its two native
/// destination pointers within the prism; normal and untouched records survive.
/// The face header receives the selected edge index and segment-region code.
std::uint32_t intersect_point_face(FeaturePrism& output, MaximumFeature& face, const MaximumFeature& point,
                                   std::array<std::uint32_t, 4> normal, bool face_is_a);

}  // namespace s3::physics::world_contact::point_feature
