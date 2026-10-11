// Port of skate-game/src/physics/offboard/grab_scene/collision.rs
//! Actual static scene swept triangles. Assembly identity comes exclusively
//! from registered physical associations, never the mesh/triangle numeric ID.
#pragma once
#include <optional>
#include <string>

#include "s3/game/physics/offboard/grab_scene.hpp"

namespace s3::game::physics::offboard::grab_scene::collision {

/// pub(super)
rs::Result<std::optional<Hit>, std::string> line(const Scene& scene, Line line);

}  // namespace s3::game::physics::offboard::grab_scene::collision
