// Port of skate-core/src/air/reckoning/math.rs.
// Original TU3 angle limiter82BD3E78 and Air heading rotation82D8DFD4.
// Basic arithmetic runs on the host; this is not bit-exact Xenon emulation.
#pragma once
#include <array>

#include "s3/core/rs.hpp"

namespace s3::air::reckoning::math {

/// pub(super)
using V = std::array<float, 4>;

/// pub(super)
float angle(V a, V b);
/// pub(super)
float wrap(float angle);
/// pub(super)
float length(V v);
/// pub(super)
V normalize(V v);
/// pub(super)
V normalize_safe(V v, V fallback);

/// 82BD3E78. Retains the source's original (not normalized) endpoint operands
/// and final target magnitude. The parallel-axis branch returns target as-is.
V limit_angle(V target, V from, float maximum);

/// 82D8DFD4..E0B0: Rodrigues columns followed by X/Y/Z ordered FMAs.
/// The native permutation repeats each column's X into its W lane. pub(super)
V rotate_heading(V heading, V up, float angle);

}  // namespace s3::air::reckoning::math
