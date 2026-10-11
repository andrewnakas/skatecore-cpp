// Port of simref sim/src/camera/world_query.rs (pub(super)).
//! Camera triangle leaf82771D08 stores the authored face normal, clamps every
//! fraction and retains the first equal-distance hit in world traversal order.
#pragma once
#include <string>

#include "s3/core/camera/positioner.hpp"
#include "s3/core/physics/board_world.hpp"

namespace s3::game::camera::world_query {

rs::Result<s3::camera::FatLineResult, std::string> line(const s3::physics::board_world::BoardWorld& world,
                                                        s3::camera::F4 start, s3::camera::F4 end, float radius);
s3::camera::FatLineResult no_hit();

}  // namespace s3::game::camera::world_query
