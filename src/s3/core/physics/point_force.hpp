// Port of skate-core/src/physics/point_force.rs.
// TU3 queued point-force application, 0x82C037DC..0x82C03908.
#pragma once
#include <cstdint>

#include "s3/core/math.hpp"

namespace s3::physics::point_force {

using math::Basis3;
using math::Vector3;

struct RetailForceAccumulator {
    Vector3 force_acceleration;
    Vector3 torque_acceleration;
    std::uint32_t cool_down = 0;
};
S3_DERIVE_EQ(RetailForceAccumulator, force_acceleration, torque_acceleration, cool_down)

/// Apply one queued force using the native multiply / fused-add order.
/// `application_point_body` already includes the collection's force-point Y
/// offset (added by the queue consumer at 0x82C037EC).
RetailForceAccumulator accumulate_point_force(RetailForceAccumulator accumulator,
                                              Vector3 force_world, Vector3 application_point_body,
                                              Basis3 deck_basis, float inverse_mass,
                                              Basis3 world_inverse_inertia);

}  // namespace s3::physics::point_force
