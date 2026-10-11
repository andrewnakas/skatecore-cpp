// Port of skate-core/src/physics/triangle_closest.rs.
// TU3 triangle closest-feature calculation82ADE8B8.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/math.hpp"

namespace s3::physics::triangle_closest {

using math::Vector3;

struct ClosestTrianglePoint {
    Vector3 point;
    /// Vertex0..2, edge AB/AC/BC3..5, face6, as used by82ADEB70.
    std::uint32_t region = 0;
    float u = 0.0f;
    float v = 0.0f;
};

ClosestTrianglePoint closest(Vector3 point, std::array<Vector3, 3> vertices);

}  // namespace s3::physics::triangle_closest
