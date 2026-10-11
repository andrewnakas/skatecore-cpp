// Port of skate-core/src/animation/vbr/transform.rs.
#include "s3/core/animation/vbr/transform.hpp"

#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/rs.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::animation::vbr::transform {

using rs::from_bits;
using rs::mul_add;

namespace {

struct Tables {
    std::array<std::array<float, 8>, 256> quantization{};
    std::array<std::array<float, 8>, 8> inverse_dct{};
};

/// Original logarithm polynomial82E89598..5DC, evaluated only at integers2..9.
/// For this bounded positive domain, exponent extraction supplies exactly the
/// integer requested by the original floor(log2-estimate) instruction pair.
float logarithm_for_frequency(std::uint32_t integer) {
    const float input = static_cast<float>(integer);
    const std::int32_t exponent = static_cast<std::int32_t>((rs::to_bits(input) >> 23) & 255u) - 127;
    const float mantissa = from_bits((rs::to_bits(input) & 0x007fffffu) | 0x3f800000u);
    const float a = mantissa - 1.0f;
    const float a2 = a * a;
    const float a3 = a * a2;
    const float a4 = a2 * a2;
    // Original vectors8230EA70 and8230EA80.
    const std::array<float, 8> c = {
        from_bits(0x3fb8aa0eu), from_bits(0xbf389e52u), from_bits(0x3ef5162du), from_bits(0xbeb1d204u),
        from_bits(0x3e77adbdu), from_bits(0xbe0cd4fbu), from_bits(0x3d5541c6u), from_bits(0xbc188b0bu),
    };
    float low = mul_add(a, c[1], c[0]);
    float high = mul_add(a, c[5], c[4]);
    low = mul_add(a2, c[2], low);
    high = mul_add(a2, c[6], high);
    low = mul_add(a3, c[3], low);
    high = mul_add(a3, c[7], high);
    const float polynomial = mul_add(a4, high, low);
    return mul_add(a, polynomial, static_cast<float>(exponent)) * from_bits(0x3f317218u);  // 82139A70
}

Tables build_tables() {
    Tables t;
    std::array<float, 8> base{};
    for (std::size_t i = 0; i < 8; ++i) {
        base[i] = logarithm_for_frequency(static_cast<std::uint32_t>(i) + 2) * from_bits(0x38000000u);
    }
    float multiplier = 1.0f;
    for (auto& row : t.quantization) {
        for (std::size_t i = 0; i < 8; ++i) row[i] = base[i] * multiplier;
        multiplier += from_bits(0x3e4ccccdu);  // 82099280; accumulated fadds
    }
    // 822F8B48..58 and individual .785398/.1.570796 literal loads.
    const std::array<std::uint32_t, 8> angles = {
        0u, 0x3ec90fdbu, 0x3f490fdbu, 0x3f96cbe4u, 0x3fc90fdbu, 0x3ffb53d2u, 0x4016cbe4u, 0x402fede0u,
    };
    for (std::size_t frame = 0; frame < 8; ++frame) {
        const float phase = static_cast<float>(frame) + 0.5f;
        std::array<float, 8> row{};
        for (std::size_t i = 0; i < 8; ++i) row[i] = trigonometry::cos(phase * from_bits(angles[i])) * 0.25f;
        row[0] *= 0.5f;
        t.inverse_dct[frame] = row;
    }
    return t;
}

const Tables& tables() {
    static const Tables TABLES = build_tables();
    return TABLES;
}

}  // namespace

float decode(std::size_t frame, std::uint8_t quantization_index, float scale, std::array<float, 8> coefficients,
             float channel_min, float channel_range) {
    const Tables& t = tables();
    const float squared = scale * scale;
    // Literal820612E0 is 2^-15. This equation was independently recovered
    // from original vmulfp/vmaddcfp instructions82E89930..4C.
    const float bias = scale * from_bits(0x38000000u);
    std::array<float, 8> weights{};
    for (std::size_t i = 0; i < 8; ++i) {
        weights[i] = mul_add(t.quantization[quantization_index][i], squared, bias) * t.inverse_dct[frame][i];
    }
    const float first = physics::native_arithmetic::dot4({coefficients[0], coefficients[1], coefficients[2], coefficients[3]},
                                                         {weights[0], weights[1], weights[2], weights[3]});
    const float last = physics::native_arithmetic::dot4({coefficients[4], coefficients[5], coefficients[6], coefficients[7]},
                                                        {weights[4], weights[5], weights[6], weights[7]});
    // BSS830CEAF0 initializer82F96D58 converts integer1 with scale1 -> .5.
    return mul_add((first + last) + 0.5f, channel_range, channel_min);
}

}  // namespace s3::animation::vbr::transform
