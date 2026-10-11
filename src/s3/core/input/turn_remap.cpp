// Port of skate-core/src/input/turn_remap.rs.
// Complete TU3 SetTurning vector remap, 82BB4248.
#include "s3/core/input/turn_remap.hpp"

#include <cstdint>

#include "s3/core/input/angle.hpp"
#include "s3/core/input/controller.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::input::turn_remap {

using rs::from_bits;
using rs::mul_add;

namespace {

float clamp(float value) {
    const float lower = -1.0f - value >= 0.0f ? -1.0f : value;
    return 1.0f - lower >= 0.0f ? lower : 1.0f;
}

float angle_of(float x, float y, float magnitude) {
    if (!(magnitude > from_bits(0x37800000u))) {
        return 0.0f;
    }
    float reciprocal = angle::reciprocal_estimate(x);
    reciprocal = mul_add(reciprocal, mul_add(-reciprocal, x, 1.0f), reciprocal);
    const float basic = angle::atan(mul_add(y, reciprocal, 0.0f));
    const std::uint32_t sign = rs::to_bits(y) & 0x80000000u;
    const float result = x < 0.0f ? from_bits(0x40490fdbu | sign) + basic : basic;
    if (x == 0.0f) {
        return from_bits(0x3fc90fdbu | sign);
    }
    return result;
}

// Scalar lane of standalone cosine 82473930; its power tree differs from SinCos.
float cosine(float angle) {
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
    const std::pair<float, std::uint32_t> terms[10] = {
        {x4, 0x3d2aaaabu},  {x6, 0xbab60b61u},  {x8, 0x37d00d01u},  {x10, 0xb493f27eu}, {x12, 0x310f76c8u},
        {x14, 0xad49cba5u}, {x16, 0x29573f9fu}, {x18, 0xa53413c3u}, {x20, 0x20f2a15du}, {x22, 0x9c8671cbu},
    };
    for (const auto& [power, coefficient] : terms) {
        value = mul_add(from_bits(coefficient), power, value);
    }
    return value;
}

}  // namespace

std::array<float, 4> TurnRemap::apply(std::array<float, 2> input) const {
    const float pi = from_bits(0x40490fdbu);
    const float magnitude_1 = controller::magnitude(input[0] * input[0] + input[1] * input[1]);
    const float first_angle = angle_of(input[0], input[1], magnitude_1);
    float graph_input = first_angle >= 0.0f ? first_angle : first_angle + pi;
    const float scaled = magnitude_1 * magnitude.evaluate(graph_input);
    const float x = clamp(cosine(first_angle) * scaled);
    const float y = clamp(trigonometry::sin(first_angle) * scaled);
    const float magnitude_2 = controller::magnitude(x * x + y * y);
    const float second_angle = angle_of(x, y, magnitude_2);
    graph_input = second_angle >= 0.0f ? second_angle : second_angle + pi;
    float mapped = angle.evaluate(graph_input);
    mapped = second_angle >= 0.0f ? mapped : mapped + pi;
    const float result_angle = angle_offset + mapped;
    return {
        clamp(cosine(result_angle) * magnitude_2),
        clamp(trigonometry::sin(result_angle) * magnitude_2),
        0.0f,
        0.0f,
    };
}

}  // namespace s3::input::turn_remap
