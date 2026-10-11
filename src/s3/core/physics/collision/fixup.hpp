// Port of skate-core/src/physics/collision/fixup.rs.
// Complete TU3 FixUpTriangleResult (82AD3130), including 82AD2CB0,
// 82AD2E00 and contact-point reprojection (82AD2BF0).
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "s3/core/math.hpp"

namespace s3::physics::collision::fixup {

using math::Vector3;

enum class TriangleRegion {
    Face,
    Edge0,
    Edge1,
    Vertex1,
    Edge2,
    Vertex0,
    Vertex2,
};

/// Native GPTriangle fields +16, +64..96, +148 and +152..160.
/// Direction order is native +64, +80, +96. Adjacency cosine indices
/// refer to the reversed direction order; these arrays are not interchangeable.
struct TriangleFeature {
    Vector3 normal;
    std::array<Vector3, 3> edges{};
    std::uint32_t flags = 0;
    std::array<float, 3> edge_cosines{};

    static constexpr std::uint32_t ONE_SIDED = 0x10;
    static constexpr std::uint32_t USE_EDGE_COSINES = 0x100;
    constexpr bool edge_convex(std::size_t edge) const { return (flags & (0x20u << edge)) != 0; }
    constexpr bool vertex_disabled(std::size_t vertex) const {
        return (flags & (0x200u << vertex)) != 0;
    }

    /// ComputeTriangleFeatureTypeFromNormal (82AD2A50), taking the normalized
    /// in-plane direction produced by FixUpTriangleResult.
    TriangleRegion classify(Vector3 direction) const;
};

struct ContactPair {
    Vector3 a;
    Vector3 b;
};
S3_DERIVE_EQ(ContactPair, a, b)

struct TriangleFixup {
    /// Whether the triangle is the other side of the pair. This flips the
    /// input for classification and chooses which contact points to project.
    bool reverse = false;
    float edge_cos_bend_normal_threshold = 0.0f;
    float convexity_epsilon = 0.0f;
    /// Native r9 flag: relaxes disabled vertices and near-flat edge handling.
    bool is_object = false;
};

/// Returns native acceptance. A successful fixup can change both the normal
/// and one side of every contact pair. Pair count is capped at native capacity.
/// (`contacts` is a Rust slice: pointer + count.)
bool fix_up_triangle(TriangleFeature triangle, Vector3& normal, ContactPair* contacts,
                     std::size_t count, TriangleFixup settings);

}  // namespace s3::physics::collision::fixup
