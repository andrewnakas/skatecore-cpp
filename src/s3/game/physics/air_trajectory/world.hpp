// Port of skate-game/src/physics/air_trajectory/world.rs.
//! Original trajectory collision leaf82771D08 and nearby triangle collector
//!82772028, across the host's world-format boundary. BoardWorld stores static
//!world-space triangles, so their source geometry transform is identity.
#pragma once
#include <array>
#include <optional>
#include <string>
#include <vector>

#include "s3/core/air/trajectory.hpp"
#include "s3/core/math.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::air_trajectory::world {

using Vector = std::array<float, 4>;
using ::s3::air::trajectory::SurfaceHit;
using ::s3::air::trajectory::WorldWithoutGrindEdges;
using ::s3::physics::board_world::BoardWorld;

inline constexpr std::array<std::array<float, 4>, 4> IDENTITY = {{
    {1.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
}};

///BoardWorld's authored format contains triangles only; no grind primitive
///collection is present. Update this adapter together with any world format
///extension that introduces actual grind edges.
inline WorldWithoutGrindEdges topology(const BoardWorld& /*world*/) { return WorldWithoutGrindEdges{}; }

rs::Result<std::optional<SurfaceHit>, std::string> line(const BoardWorld& world, Vector start, Vector end,
                                                        float radius);
rs::Result<std::vector<std::array<Vector, 3>>, std::string> nearby(const BoardWorld& world, Vector center,
                                                                  float radius);

// Private helpers in Rust.
///82761698 full separating-axis triangle/AABB predicate: nine edge-axis
///crosses, three box axes and the face plane. No broadphase-only substitution.
bool triangle_box(std::array<Vector, 3> vertices, Vector minimum, Vector maximum);
bool separated(std::array<Vector, 3> vertices, Vector axis, Vector half);
float dot(Vector a, Vector b);
Vector sub(Vector a, Vector b);
Vector cross(Vector a, Vector b);
::s3::math::Vector3 vec3(Vector v);
Vector lanes(::s3::math::Vector3 v);

}  // namespace s3::game::physics::air_trajectory::world
