// Port of skate-core/src/camera/orientation_math.rs (pub(super) items).
//! Native camera Euler conversions. These preserve the products and fused sums
//! from TU3 82E06CB8 and 82E045E0 rather than passing through renderer math.
#pragma once
#include <array>
#include <cstddef>

#include "s3/core/camera/_types.hpp"
#include "s3/core/math.hpp"
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/reciprocal_sqrt.hpp"

namespace s3::camera::orientation_math {

using math::Basis3;

F4 quaternion_from_angles(float pitch, float yaw, float roll);

/// TU3 82E04AD4..82E04BC4. The native matrix's three spatial columns are
/// retained; its unused fourth lanes are not part of the host Basis3 type.
Basis3 basis_from_angles(float pitch, float yaw, float roll);

/// Complete axis rotation82BD35B8. Standalone sine/cosine and the matrix
/// product's X,Y,Z accumulation order are observable in the camera output.
Basis3 rotate_about_axis(const Basis3& basis, std::array<float, 3> axis, float angle);

/// Look basis8296EF20: normalize world-up cross At, then normalize At cross
/// Right. The caller has already handled a zero direction and its pitch cap.
Basis3 look_basis(std::array<float, 3> at);

template <std::size_t N>
std::array<float, N> normalize(std::array<float, N> v) {
    const float square = physics::native_arithmetic::dot3(F4{v[0], v[1], v[2], 0.0f}, F4{v[0], v[1], v[2], 0.0f});
    float inverse = physics::reciprocal_sqrt::estimate(square);
    for (int i = 0; i < 2; ++i) {
        inverse = rs::mul_add(inverse * 0.5f, rs::mul_add(-square, inverse * inverse, 1.0f), inverse);
    }
    return rs::map(v, [&](float lane) { return lane * inverse; });
}

}  // namespace s3::camera::orientation_math
