// Port of skate-core/src/physics/collision/edge.rs.
// TU3 EdgeCosTest / EdgeCosTestFull, 82AD2860 / 82AD2928.
#pragma once
#include "s3/core/physics/collision/arithmetic.hpp"

namespace s3::physics::collision::edge {

inline bool edge_cos_test(math::Vector3 edge, math::Vector3 face, math::Vector3 normal, float cosine,
                          bool convex, bool two_sided) {
    using namespace arithmetic;
    if (dot(normal, cross(edge, face)) < 0.0f) {
        return true;
    }
    if (!convex && !two_sided) {
        return false;
    }
    const V projected = normalize(sub(normal, scale(edge, dot(normal, edge))), 2);
    return dot(projected, convex ? face : neg(face)) >= cosine;
}

}  // namespace s3::physics::collision::edge
