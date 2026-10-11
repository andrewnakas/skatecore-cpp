// Port of skate-core/src/trigonometry.rs.
//
// Coefficients are the executable words at 0x822F97C0..0x822F9860.
// Arithmetic follows the recovered polynomial structure. Numerical agreement
// with the original game requires independent validation.
#include "s3/core/trigonometry.hpp"

#include <cstdint>

#include "s3/core/physics/reciprocal_sqrt.hpp"
#include "s3/core/rs.hpp"

namespace s3::trigonometry {

using rs::from_bits;
using rs::mul_add;

std::pair<float, float> sin_cos(float angle) {
    const float turns = rs::round_ties_even(angle * from_bits(0x3E22F983u));
    const float x = mul_add(-from_bits(0x40C90FDBu), turns, angle);
    const float x2 = x * x;
    const float x3 = x2 * x;
    const float x4 = x2 * x2;
    const float x5 = x3 * x2;
    const float x6 = x3 * x3;
    const float x7 = x4 * x3;
    const float x8 = x4 * x4;
    const float x9 = x5 * x4;
    const float x10 = x5 * x5;
    const float x11 = x6 * x5;
    const float x12 = x6 * x6;
    const float x13 = x7 * x6;
    const float x15 = x8 * x7;
    const float x14 = x7 * x7;
    const float x16 = x8 * x8;
    const float x17 = x9 * x8;
    const float x19 = x10 * x9;
    const float x18 = x9 * x9;
    const float x21 = x11 * x10;
    const float x20 = x10 * x10;
    const float x23 = x12 * x11;
    const float x22 = x11 * x11;

    float sin = mul_add(from_bits(0xBE2AAAABu), x3, x);
    sin = mul_add(from_bits(0x3C088889u), x5, sin);
    sin = mul_add(x7, from_bits(0xB9500D01u), sin);
    sin = mul_add(from_bits(0x3638EF1Du), x9, sin);
    sin = mul_add(from_bits(0xB2D7322Bu), x11, sin);
    sin = mul_add(from_bits(0x2F309231u), x13, sin);
    sin = mul_add(from_bits(0xAB573F9Fu), x15, sin);
    sin = mul_add(from_bits(0x274A963Cu), x17, sin);
    sin = mul_add(from_bits(0xA317A4DAu), x19, sin);
    sin = mul_add(from_bits(0x1EB8DC78u), x21, sin);
    sin = mul_add(from_bits(0x9A3B0DA1u), x23, sin);

    float cos = mul_add(-0.5f, x2, 1.0f);
    cos = mul_add(from_bits(0x3D2AAAABu), x4, cos);
    cos = mul_add(from_bits(0xBAB60B61u), x6, cos);
    cos = mul_add(from_bits(0x37D00D01u), x8, cos);
    cos = mul_add(from_bits(0xB493F27Eu), x10, cos);
    cos = mul_add(from_bits(0x310F76C8u), x12, cos);
    cos = mul_add(from_bits(0xAD49CBA5u), x14, cos);
    cos = mul_add(from_bits(0x29573F9Fu), x16, cos);
    cos = mul_add(from_bits(0xA53413C3u), x18, cos);
    cos = mul_add(from_bits(0x20F2A15Du), x20, cos);
    cos = mul_add(from_bits(0x9C8671CBu), x22, cos);
    return {sin, cos};
}

/// Standalone TU3 cosine82473930. The even-power tree differs from SinCos;
/// normal gameplay camera roll82BD35B8 calls this function independently.
/// This is the retained readable polynomial also audited for SetTurning.
float cos(float angle) {
    const float turns = rs::round_ties_even(angle * from_bits(0x3e22f983u));
    const float x = mul_add(-from_bits(0x40c90fdbu), turns, angle);
    const float x2 = x * x;
    const float x4 = x2 * x2;
    const float x6 = x4 * x2;
    const float x8 = x4 * x4;
    const float x10 = x6 * x4;
    const float x12 = x6 * x6;
    const float x14 = x8 * x6;
    const float x16 = x8 * x8;
    const float x18 = x10 * x8;
    const float x22 = x12 * x10;
    const float x20 = x10 * x10;
    float value = mul_add(-0.5f, x2, 1.0f);
    const std::pair<float, std::uint32_t> terms[] = {
        {x4, 0x3d2aaaabu},  {x6, 0xbab60b61u},  {x8, 0x37d00d01u},  {x10, 0xb493f27eu},
        {x12, 0x310f76c8u}, {x14, 0xad49cba5u}, {x16, 0x29573f9fu}, {x18, 0xa53413c3u},
        {x20, 0x20f2a15du}, {x22, 0x9c8671cbu},
    };
    for (const auto& [power, coefficient] : terms) {
        value = mul_add(from_bits(coefficient), power, value);
    }
    return value;
}

/// TU3 vector sine 824531C8. Its sequential odd powers differ from SinCos's
/// multiplication tree, so callers of the standalone sine must use this path.
float sin(float angle) {
    const float turns = rs::round_ties_even(angle * from_bits(0x3E22F983u));
    const float x = mul_add(-from_bits(0x40C90FDBu), turns, angle);
    const float x2 = x * x;
    float power = x * x2;
    float result = x;
    const std::uint32_t coefficients[] = {
        0xBE2AAAABu, 0x3C088889u, 0xB9500D01u, 0x3638EF1Du, 0xB2D7322Bu, 0x2F309231u,
        0xAB573F9Fu, 0x274A963Cu, 0xA317A4DAu, 0x1EB8DC78u, 0x9A3B0DA1u,
    };
    for (std::uint32_t coefficient : coefficients) {
        result = mul_add(from_bits(coefficient), power, result);
        power *= x2;
    }
    return result;
}

/// Scalar lane of TU3 vector acos 82453298. The native radicand constant is
/// 0x3f800001, and its reciprocal square root receives one refinement.
float acos(float value) { return (from_bits(0x40490FDBu) * 0.5f) - asin(value); }

/// Shared inverse-sine polynomial, also inlined by DisablePushBrake82BA5150.
/// Keep this intermediate separate: that caller subtracts degrees from90,
/// whereas vector acos subtracts radians from half pi.
float asin(float value) {
    const float a = rs::abs(value);
    const float cube = (value * value) * a;
    auto c = [](std::uint32_t b) { return from_bits(b); };
    float p0 = mul_add(c(0x400B1889u), a, c(0xC0D1360Eu));
    float p1 = mul_add(c(0x3E663246u), a, c(0xBF983F2Fu));
    float p2 = mul_add(c(0xBED65553u), a, c(0x408980BDu));
    float p3 = mul_add(c(0xBD6DD42Du), a, c(0x3F1DD7B6u));
    p0 = mul_add(p0, a, c(0x40AF6AD8u));
    p1 = mul_add(p1, a, c(0x3FB58485u));
    p2 = mul_add(p2, a, c(0xC08F6AD9u));
    p3 = mul_add(p3, a, c(0xBFAF4418u));
    const float left = mul_add(p1, cube, p0);
    const float right = mul_add(p3, cube, p2);
    const float radicand = c(0x3F800001u) - a;
    float r = physics::reciprocal_sqrt::estimate(radicand);
    const float correction = mul_add(-(radicand * 0.5f), r * r, 0.5f);
    r = mul_add(r, correction, r);
    const float residual = mul_add(-a, value, value) * right;
    return mul_add(residual, r, value * left);
}

}  // namespace s3::trigonometry
