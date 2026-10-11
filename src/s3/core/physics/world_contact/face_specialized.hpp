// Port of skate-core/src/physics/world_contact/face_specialized.rs
// (module physics::world_contact::dispatch::face_specialized via #[path]).
#pragma once
#include "s3/core/physics/world_contact/arithmetic.hpp"

namespace s3::physics::world_contact::dispatch::face_specialized {

/// TU3 82ACCEF0. The quad owns the outer edge-intersection loop even when it is
/// the second feature in the caller's pair. This order reaches contact reduction.
std::uint32_t quad_triangle(FeaturePrism& output, const MaximumFeature& quad, const MaximumFeature& triangle,
                            std::array<std::uint32_t, 4> normal, bool quad_is_a);

/// TU3 82ACD7D8. Its calculations match 82ACCEF0 with four inner edges and four
/// vertices in both containment passes; the source transposes these into lanes.
std::uint32_t quad_quad(FeaturePrism& output, const MaximumFeature& a, const MaximumFeature& b,
                        std::array<std::uint32_t, 4> normal);

}  // namespace s3::physics::world_contact::dispatch::face_specialized
