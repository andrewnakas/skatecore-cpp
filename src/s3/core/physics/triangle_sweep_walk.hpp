// Port of skate-core/src/physics/triangle_sweep_walk.rs.
// Five-step vertex/edge region walk82ADF008..82ADF5BC.
#pragma once
#include "s3/core/physics/triangle_closest.hpp"
#include "s3/core/physics/triangle_query.hpp"

namespace s3::physics::triangle_sweep_walk {

bool walk(triangle_query::TriangleLineHit& result, math::Vector3 start, math::Vector3 delta,
          std::array<math::Vector3, 3> vertices, triangle_closest::ClosestTrianglePoint closest,
          float radius);

}  // namespace s3::physics::triangle_sweep_walk
