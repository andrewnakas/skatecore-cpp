// Port of skate-core/src/physics/construction/principal_axes.rs (module physics::mass::principal_axes).
// TU3 inertia diagonalization: 82AE8388 and its three rotation helpers.
// Rows here are mathematical rows. The caller transposes the resulting axes
// into RenderWare's Ri/Up/At storage. No eigenvalue sorting is performed.
#pragma once
#include <array>

namespace s3::physics::mass::principal_axes {

// pub(super)
using Matrix3 = std::array<std::array<float, 3>, 3>;

// pub(super)
Matrix3 diagonalize(Matrix3& inertia);

}  // namespace s3::physics::mass::principal_axes
