// Port of skate-core/src/physics/collision/sphere_triangle.rs.
// Sphere/triangle execution path of PrimitivePairIntersect (82AD3CD8).
// Both primitive orders, two-sided geometry, finite edges, vertices, fatness,
// separating-direction refinement and complete triangle fixups are preserved.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/physics/collision/fixup.hpp"

namespace s3::physics::collision::sphere_triangle {

using fixup::ContactPair;
using fixup::TriangleFeature;
using math::Vector3;

struct Sphere {
    Vector3 center;
    float radius = 0.0f;
};

/// A world-space GPTriangle. Normal, unit directions and lengths are produced
/// by the native collision-geometry loader; rendering normals are not suitable.
struct Triangle {
    std::array<Vector3, 3> vertices{};
    TriangleFeature feature;
    std::array<float, 3> edge_lengths{};
    float fatness = 0.0f;
};

struct SphereTrianglePair {
    Vector3 separating_direction;
    float separating_distance = 0.0f;
    /// Pair normal points from primitive A toward primitive B. The downstream
    /// physics contact producer owns its conversion to solver normal convention.
    Vector3 normal;
    ContactPair points;
    float distance = 0.0f;
    std::uint32_t triangle_region = 0;
};

/// Native pair query on already-built GP primitives. `triangle_is_a` selects
/// primitive order. An accepted candidate can have positive distance: filtering
/// candidate contacts belongs to the caller, just as in TU3.
std::optional<SphereTrianglePair> intersect_sphere_triangle(Sphere sphere, Triangle triangle,
                                                            float minimum_separating_distance,
                                                            bool triangle_is_a);

// pub(super)
struct SphereTrianglePrism {
    Vector3 separating_direction;
    float separating_distance = 0.0f;
    Vector3 normal;
    ContactPair raw_points;
    std::uint32_t triangle_region = 0;
};

/// Shared SAT/features/prism work. The two native callers have different
/// single-point refinement gates and publish their results in different order.
std::optional<SphereTrianglePrism> build_prism(Sphere sphere, Triangle triangle,
                                               float minimum_separating_distance, bool triangle_is_a,
                                               bool contact_query);

}  // namespace s3::physics::collision::sphere_triangle
