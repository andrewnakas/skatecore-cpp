// Port of skate-game/src/physics/slide_state/update.rs.
// NOTE: namespace is `update_` (not `update`) because the parent declares free fn `slide_state::update`.
//! Ordered UpdateSkateboard82D3A900: steering, manual, wall launch or Slide forces.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/math.hpp"
#include "s3/core/physics/force_queue.hpp"
#include "s3/core/riding/pumping/geometry.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::slide_state::update_ {

/// `pub(super)`.
Status board(GamePhysics& physics, SkaterRuntime& skater);

/// Private: `impl ManualAngleMeasurement for Angle` (Error = Infallible; duck-typed).
struct Angle {
    using Error = sc::riding::pumping::geometry::Infallible;
    rs::Result<float, Error> angle_between(std::array<float, 4> a, std::array<float, 4> b, std::array<float, 4> c);
};

/// Private helpers.
sc::physics::force_queue::QueuedPointForce point_force(std::uint32_t tag, std::array<float, 8> f);
sc::math::Vector3 xyz(std::array<float, 4> v);
std::array<float, 4> raw(std::array<std::uint32_t, 4> v);
float fsel(float test, float a, float b);

}  // namespace s3::game::physics::slide_state::update_
