// Port of skate-core/src/physics/contact_feedback/compression.rs.
#include "s3/core/physics/contact_feedback/compression.hpp"

#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::physics::contact_feedback::compression {

namespace {
M matrix(const PoseMatrix& words) {
    M m{};
    for (std::size_t i = 0; i < 4; ++i)
        for (std::size_t j = 0; j < 4; ++j) m[i][j] = rs::from_bits(words[i * 4 + j]);
    return m;
}
V madd(const V& a, float s, const V& b) {
    return V{rs::mul_add(a[0], s, b[0]), rs::mul_add(a[1], s, b[1]), rs::mul_add(a[2], s, b[2]),
             rs::mul_add(a[3], s, b[3])};
}
V scale(const V& a, float s) { return V{a[0] * s, a[1] * s, a[2] * s, a[3] * s}; }
V cross(const V& a, const V& b) {
    return V{
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
        rs::mul_add(-a[3], b[3], a[3] * b[3]),
    };
}
float dot(const V& a, const V& b) { return native_arithmetic::dot3(a, b); }
float reciprocal_estimate(float value) { return native_arithmetic::reciprocal_estimate(value); }
}  // namespace

PoseMatrix part_mass_transform(const PartPose& part) {
    if (part.body) {
        const auto& body = *part.body;
        static constexpr std::size_t base[4] = {16, 20, 24, 4};
        PoseMatrix out{};
        for (std::size_t i = 0; i < 16; ++i) out[i] = body[base[i / 4] + i % 4];
        return out;
    }
    if (!part.local_mass_frame) {
        return part.transform;
    }
    const M local = matrix(*part.local_mass_frame);
    std::array<V, 3> basis{};
    for (std::size_t i = 0; i < 3; ++i) basis[i] = V{local[0][i], local[1][i], local[2][i], 0.0f};
    const V neg{0.0f - local[3][0], 0.0f - local[3][1], 0.0f - local[3][2], 0.0f - local[3][3]};
    const V position = madd(basis[0], neg[0], madd(basis[1], neg[1], scale(basis[2], neg[2])));
    const M cached = matrix(part.transform);
    M result{};
    for (std::size_t i = 0; i < 4; ++i) {
        const V v = i == 3 ? position : basis[i];
        const V first = i == 3 ? madd(cached[0], v[0], cached[3]) : scale(cached[0], v[0]);
        result[i] = madd(cached[2], v[2], madd(cached[1], v[1], first));
    }
    PoseMatrix out{};
    for (std::size_t i = 0; i < 16; ++i) out[i] = rs::to_bits(result[i / 4][i % 4]);
    return out;
}

void calculate_wheel_compressions(std::array<std::uint8_t, 8400>& board, const std::array<PartPose, 7>& parts) {
    const M deck = matrix(part_mass_transform(parts[6]));
    std::array<V, 4> wheels{};
    for (std::size_t i = 0; i < 4; ++i) wheels[i] = matrix(part_mass_transform(parts[i]))[3];
    const std::uint32_t distance_word = (std::uint32_t(board[8380]) << 24) | (std::uint32_t(board[8381]) << 16) |
                                        (std::uint32_t(board[8382]) << 8) | std::uint32_t(board[8383]);
    const float distance = rs::from_bits(distance_word);
    const auto heights = average_wheel_compressions(deck, wheels, distance);
    const std::pair<std::size_t, float> outputs[2] = {{8372, heights[0]}, {8376, heights[1]}};
    for (const auto& [offset, height] : outputs) {
        const std::uint32_t word = rs::is_nan(height) ? (rs::to_bits(height) | 0x00400000u) : rs::to_bits(height);
        const auto bytes = rs::to_be_bytes_u32(word);
        for (std::size_t i = 0; i < 4; ++i) board[offset + i] = bytes[i];
    }
}

std::array<float, 2> average_wheel_compressions(M deck, std::array<V, 4> wheel_positions, float rest_height) {
    const V c0 = cross(deck[1], deck[2]);
    const V c1 = cross(deck[2], deck[0]);
    const V c2 = cross(deck[0], deck[1]);
    const float determinant = dot(deck[0], c0);
    // Native reciprocal estimate and two refinements.
    float inverse = reciprocal_estimate(determinant);
    for (int k = 0; k < 2; ++k) {
        inverse = rs::mul_add(inverse, rs::mul_add(-inverse, determinant, 1.0f), inverse);
    }
    std::array<V, 3> basis{};
    for (std::size_t i = 0; i < 3; ++i) basis[i] = scale(V{c0[i], c1[i], c2[i], c1[i]}, inverse);
    V neg{};
    for (std::size_t i = 0; i < 4; ++i) neg[i] = rs::from_bits(rs::to_bits(deck[3][i]) ^ 0x80000000u);
    const V position = madd(basis[2], neg[2], madd(basis[1], neg[1], scale(basis[0], neg[0])));
    std::array<float, 4> heights{};
    for (std::size_t i = 0; i < 4; ++i) {
        const V p = wheel_positions[i];
        const V local = madd(basis[2], p[2], madd(basis[1], p[1], madd(basis[0], p[0], position)));
        heights[i] = local[1];
    }
    return {
        (heights[0] + heights[1]) * 0.5f - rest_height,
        (heights[2] + heights[3]) * 0.5f - rest_height,
    };
}

}  // namespace s3::physics::contact_feedback::compression
