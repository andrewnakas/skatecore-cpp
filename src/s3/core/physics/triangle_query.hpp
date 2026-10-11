// Port of skate-core/src/physics/triangle_query.rs.
// Native triangle segment queries used by wheel probes and camera sweeps.
// Thin test82AC7858 and dispatcher82ADF5D8, TU3. The world owns candidate
// storage; these routines retain the source winding, tolerances and arithmetic.
#pragma once
#include <array>
#include <optional>

#include "s3/core/math.hpp"

namespace s3::physics::triangle_query {

using math::Vector3;

struct TriangleLineHit {
    Vector3 position;
    Vector3 normal;
    float fraction = 0.0f;
    /// Native result48.xy: weights for vertices1 and2; z is penetration data.
    std::array<float, 3> volume_parameter{};
};
S3_DERIVE_EQ(TriangleLineHit, position, normal, fraction, volume_parameter)

/// TU3 dispatcher82ADF5D8. The rounded query uses the sum of the two radii,
/// then shifts its center hit back by the query radius. As in the native output
/// structure, fields not written by a branch retain the caller's values.
bool triangle_segment(TriangleLineHit& result, Vector3 start, Vector3 direction,
                      std::array<Vector3, 3> vertices, float line_radius, float triangle_fatness);

/// Complete zero-total-radius branch. `direction` is end minus start, not a
/// normalized ray. TU3 accepts a small margin beyond both segment endpoints;
/// do not clamp its returned fraction or replace the determinant tolerance.
std::optional<TriangleLineHit> thin_triangle(Vector3 start, Vector3 direction,
                                             std::array<Vector3, 3> vertices);

// pub(super) helpers.
float dot(Vector3 a, Vector3 b);
Vector3 sub(Vector3 a, Vector3 b);
Vector3 scale(Vector3 v, float scale);
Vector3 madd(Vector3 a, float scale, Vector3 b);
Vector3 cross(Vector3 a, Vector3 b);
float inverse_length(float squared);

}  // namespace s3::physics::triangle_query
