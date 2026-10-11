// Port of skate-game/src/physics/colliders.rs.
//! Bind stock child geometry to the live solver part transforms.
#pragma once
#include <vector>

#include "s3/core/math.hpp"
#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/physics/world_contact.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::settings {
struct PhysicsSettings;  // physics/settings.hpp (coordinator)
}

namespace s3::game::physics::colliders {

std::vector<sc::physics::board_world::BoardWorldVolume> world_volumes(
    const sc::physics::board_runtime::BoardRuntime& board, const settings::PhysicsSettings& settings);

/// Private in Rust.
sc::physics::world_contact::ContactPrimitive capsule(sc::physics::drive_frames::RetailAffineTransform pose,
                                                     float radius, float half_length);
/// Private in Rust. Same multiply/add order as the Volume-to-world vertex path82ADE468.
sc::math::Vector3 transform(sc::math::Basis3 basis, sc::math::Vector3 point, sc::math::Vector3 origin);
/// Private in Rust.
sc::physics::drive_frames::RetailAffineTransform compose(sc::physics::drive_frames::RetailAffineTransform parent,
                                                         sc::physics::drive_frames::RetailAffineTransform child);

}  // namespace s3::game::physics::colliders
