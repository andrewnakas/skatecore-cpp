// Port of skate-core/src/player/offboard/ground_input/math.rs (whole file).
// Independently derived from original82D310F8 and82473B98.
// Ordinary PC reciprocal/sqrt seeds retain original refinements; they are not
// claimed to reproduce Xenon estimate instructions bit for bit.
#pragma once
#include <array>

#include "s3/core/rs.hpp"

namespace s3::player::offboard::ground_input::math {

using Vector = std::array<float, 3>;

inline float dot(Vector a, Vector b) {
    // PC dot3, without a hardware VMX sum-rounding parity claim.
    return (a[0] * b[0] + a[1] * b[1]) + a[2] * b[2];
}
inline Vector cross(Vector a, Vector b) {
    return Vector{rs::mul_add(-a[2], b[1], a[1] * b[2]), rs::mul_add(-a[0], b[2], a[2] * b[0]),
                  rs::mul_add(-a[1], b[0], a[0] * b[1])};
}
Vector normalize(Vector v);
float length(Vector v);
Vector inverse_frame(Vector right, Vector up, Vector forward, Vector v);
float signed_angle(float x, float y);
/// 82473B98: range reduction, rational polynomial, strict thresholds and signs.
float rational_atan(float original);

}  // namespace s3::player::offboard::ground_input::math
