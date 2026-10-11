// Port of skate-core/src/physics/solver/contact_build/publication.rs.
// Scaled Jacobian and restitution target publication, `82AE1590..82AE15F4`.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/physics/solver/contact_build.hpp"

namespace s3::physics::solver::contact_build::publication {

/// pub(super)
std::array<std::uint32_t, 64> compile(const ContactPreparation& prepared,
                                      std::array<float, 3> inverse_response);

}  // namespace s3::physics::solver::contact_build::publication
