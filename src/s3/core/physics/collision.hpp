// Port of skate-core/src/physics/collision/mod.rs.
// TU3 feature-prism collision. Geometry and triangle adjacency come from the
// collision mesh, independently of rendering and material response.
#pragma once
#include "s3/core/physics/collision/fixup.hpp"
#include "s3/core/physics/collision/sphere_triangle.hpp"
#include "s3/core/physics/collision/world_contact.hpp"

namespace s3::physics::collision {
using fixup::ContactPair;
using fixup::fix_up_triangle;
using fixup::TriangleFeature;
using fixup::TriangleFixup;
using fixup::TriangleRegion;
using sphere_triangle::intersect_sphere_triangle;
using sphere_triangle::Sphere;
using sphere_triangle::SphereTrianglePair;
using sphere_triangle::Triangle;
using world_contact::sphere_triangle_world_contact;
using world_contact::world_separation_limit;
using world_contact::WorldContact;
using world_contact::WorldContactSettings;
}  // namespace s3::physics::collision
