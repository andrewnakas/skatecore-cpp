// Port of skate-core/src/physics/deck_angular_correction.rs.
// Deck angular corrections from TU3 82C07000 and 82C075B8.
// These add angular acceleration before the shared solve. They never set yaw
// or orientation directly. Both native fixed-step conversions are retained.
#pragma once

#include "s3/core/math.hpp"
#include "s3/core/physics/rigid_body.hpp"

namespace s3::physics::deck_angular_correction {

using math::Basis3;
using math::Vector3;
using rigid_body::RetailBodyRates;

/// 82C07328 subtracts the current angular displacement along the requested
/// axis, without07000's directional clamps, then performs the same tensor and
/// fixed-step conversion as075B8. The thrown-board controller calls this leaf.
void apply_axis_displacement(RetailBodyRates& body, Vector3 requested);

/// 82C07000 first limits the requested angular displacement against the
/// displacement already supplied by angular velocity along that same axis.
void apply_limited_displacement(RetailBodyRates& body, Vector3 requested);

/// 82C075B8. The inverse of the current world inverse-inertia tensor converts
/// displacement/step to torque; another step division and the original tensor
/// convert it to angular acceleration. Do not algebraically cancel the tensor
/// pair: its rounded cofactors and ordered products are observable.
void apply_angular_displacement(RetailBodyRates& body, Vector3 displacement);

/// Ground board update82D389DC..82D38C98. Skateboard+16 is the cached deck
/// rigid body (ctor82C01278); this torque therefore enters the same accumulator.
/// The inline pow calculation has fixed inputs10 and-1. Retain its two
/// coefficient trees and reciprocal refinements instead of replacing it by.1.
void apply_ground_body_torque(RetailBodyRates& body);

}  // namespace s3::physics::deck_angular_correction
