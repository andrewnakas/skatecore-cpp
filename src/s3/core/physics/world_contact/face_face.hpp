// Port of skate-core/src/physics/world_contact/face_face.rs
// (module physics::world_contact::dispatch::face_face via #[path]).
#pragma once
#include "s3/core/physics/world_contact/arithmetic.hpp"

namespace s3::physics::world_contact::dispatch::face_face {

/// General convex-face branch at 82ACE3D8, after specialized paths decline.
/// Retains native candidate order: B vertices, edge crossings, then A vertices.
/// Plane-touching vertices/crossings are included without deduplication here.
std::uint32_t intersect(FeaturePrism& output, MaximumFeature& a, MaximumFeature& b);

}  // namespace s3::physics::world_contact::dispatch::face_face
