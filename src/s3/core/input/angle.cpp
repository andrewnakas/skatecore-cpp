// Port of skate-core/src/input/angle.rs.
// Scalar lane of native SIMD atan 82473B98 and intention left-stick angle.
#include "s3/core/input/angle.hpp"

#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/rs.hpp"

namespace s3::input::angle {

using rs::from_bits;
using rs::mul_add;

float reciprocal_estimate(float value) { return physics::native_arithmetic::reciprocal_estimate(value); }

namespace {
float refined_reciprocal(float value) {
    const float initial = reciprocal_estimate(value);
    const float first = mul_add(initial, mul_add(-value, initial, 1.0f), initial);
    const float second = mul_add(first, mul_add(-value, first, 1.0f), first);
    return rs::is_nan(first) ? initial : second;
}
}  // namespace

float atan(float value) {
    const float absolute = rs::abs(value);
    const bool invert = absolute > 1.0f;
    const float reduced = invert ? refined_reciprocal(absolute) : absolute;
    const bool transform = reduced > from_bits(0x3e8930a3u);
    const float offset = transform ? (invert ? from_bits(0x3f860a92u) : from_bits(0x3f060a92u))
                                   : (invert ? from_bits(0x3fc90fdbu) : 0.0f);
    const float transformed = (mul_add(reduced, from_bits(0x3f3b67afu), reduced) - 1.0f) *
                              refined_reciprocal(reduced + from_bits(0x3fddb3d7u));
    const float t = transform ? transformed : reduced;
    const float square = t * t;
    float numerator = mul_add(square, from_bits(0xbf566bd7u), from_bits(0xc107e9fbu));
    numerator = mul_add(square, numerator, from_bits(0xc1a40bfeu));
    numerator = mul_add(square, numerator, from_bits(0xc15b0533u));
    float denominator = mul_add(square, square + from_bits(0x4170624fu), from_bits(0x426e5052u));
    denominator = mul_add(square, denominator, from_bits(0x42ac5090u));
    denominator = mul_add(square, denominator, from_bits(0x422443e6u));
    const float polynomial = mul_add(t, (numerator * square) * refined_reciprocal(denominator), t);
    float result = from_bits(0x39800000u) > rs::abs(t) ? t : polynomial;
    result = (invert ? -result : result) + offset;
    result = value < 0.0f ? -result : result;
    result = value > from_bits(0x7e800000u) ? from_bits(0x3fc90fdbu) : result;
    if (-from_bits(0x7e800000u) > value) {
        return -from_bits(0x3fc90fdbu);
    }
    return result;
}

float left_stick_angle(float x, float y) {
    if (x == 0.0f && y == 0.0f) {
        return 0.0f;
    }
    const float vertical = -y;
    const float reciprocal = reciprocal_estimate(vertical);
    const float refined = mul_add(reciprocal, mul_add(-reciprocal, vertical, 1.0f), reciprocal);
    const float basic = atan(mul_add(x, refined, 0.0f));
    const std::uint32_t sign = rs::to_bits(x) & 0x80000000u;
    const float pi = from_bits(0x40490fdbu | sign);
    const float half_pi = from_bits(0x3fc90fdbu | sign);
    const float angle = 0.0f > vertical ? pi + basic : basic;
    return vertical == 0.0f ? half_pi : angle;
}

}  // namespace s3::input::angle
