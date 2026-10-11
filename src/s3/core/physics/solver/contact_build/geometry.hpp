// Port of skate-core/src/physics/solver/contact_build/geometry.rs.
// Contact arms and inverse-inertia response, `82AE11BC..82AE1588`.
#pragma once
#include "s3/core/physics/solver/contact_build.hpp"
#include "s3/core/physics/solver/contact_build/input.hpp"

namespace s3::physics::solver::contact_build::geometry {

/// pub(super)
ContactPreparation prepare(const input::ContactInput& input, float time_step);

}  // namespace s3::physics::solver::contact_build::geometry
