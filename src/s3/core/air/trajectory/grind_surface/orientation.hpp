// Port of skate-core/src/air/trajectory/grind_surface/orientation.rs.
#pragma once
#include <array>

namespace s3::air::trajectory::grind_surface::orientation {

using V = std::array<float, 4>;

/// 82C21828. Strict straddling test and nearest-limit tie-to-high branch.
V tilted_normal(V direction, V up, std::array<float, 2> limits);

/// 82C1E220: half-angle sine/cosine quaternion; existing independent PC
/// trigonometry supplies its numerical leaves, not a generated SIMD helper.
V rotate(V axis, V value, float angle);

}  // namespace s3::air::trajectory::grind_surface::orientation
