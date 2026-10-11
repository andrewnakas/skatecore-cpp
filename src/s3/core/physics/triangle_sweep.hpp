// Port of skate-core/src/physics/triangle_sweep.rs.
// Rounded triangle query82ADEB70: bounds, face, overlap, then feature walk.
#pragma once
#include "s3/core/physics/triangle_query.hpp"

namespace s3::physics::triangle_sweep {

using math::Vector3;
using triangle_query::TriangleLineHit;

/// Literal8212BF74, also used by closest-feature and walk denominator gates.
inline const float MIN_RECIPROCAL = rs::from_bits(0x00200000u);

bool sweep(TriangleLineHit& result, Vector3 start, Vector3 delta, std::array<Vector3, 3> vertices,
           float radius);

Vector3 add(Vector3 a, Vector3 b);

}  // namespace s3::physics::triangle_sweep
