// Port of skate-core/src/animation/foot_ik/post_contact.rs.
#include "s3/core/animation/foot_ik/post_contact.hpp"

#include "s3/core/animation/foot_ik/math.hpp"
#include "s3/core/animation/foot_ik/transforms.hpp"
#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::animation::foot_ik::post_contact {

using math::reciprocal;
using physics::native_arithmetic::vector_min;
using physics::skeleton_animation_record::transform_point;
using rs::from_bits;
using rs::mul_add;
using transforms::inverse_rigid;

std::optional<std::array<float, 4>> target(const Transform& board, std::array<float, 4> foot,
                                           std::array<float, 4> adjacent,
                                           std::array<float, 4> animated_board_position, bool wipeout,
                                           const Settings& settings, const SettingsPost& post) {
    const Transform inverse = inverse_rigid(board);
    const std::array<float, 4> local_foot = transform_point(inverse, foot);
    const std::array<float, 4> local_adjacent = transform_point(inverse, adjacent);
    std::array<float, 4> midpoint{};
    for (std::size_t i = 0; i < 4; ++i) midpoint[i] = (local_foot[i] + local_adjacent[i]) * 0.5f;
    //vrlimi mask4 preserves the foot's Y; X/Z test both foot and midpoint.
    midpoint[1] = local_foot[1];
    const float x = vector_min(rs::abs(local_foot[0]), rs::abs(midpoint[0]));
    const float z = vector_min(rs::abs(local_foot[2]), rs::abs(midpoint[2]));
    if (x >= settings.deck_half_width + settings.post_ik_padding[0] ||
        z >= settings.deck_total_half_length + settings.post_ik_padding[2]) {
        return std::nullopt;
    }
    float height = wipeout ? post.wipeout_height : post.riding_height;
    if (z > settings.deck_half_length) {
        float distance = z - settings.deck_half_length;
        const float end = settings.deck_total_half_length - settings.deck_half_length;
        distance = end - distance >= 0.0f ? distance : end;
        //Constructor82BED418 caches tan(clamp(authored degrees*.01745...)).
        float angle = settings.deck_front_angle_degrees * from_bits(0x3C8EFA35u);
        angle = 0.01f - angle >= 0.0f ? 0.01f : angle;
        const float maximum = from_bits(0x3FC7C82Du);
        angle = maximum - angle >= 0.0f ? angle : maximum;
        height = mul_add(distance, tangent(angle), height);
    }
    if (midpoint[1] >= height || midpoint[1] <= height - from_bits(0x3E99999Au)) {
        return std::nullopt;
    }
    std::array<float, 4> result = animated_board_position;
    result[1] = height;
    return transform_point(board, result);
}

float tangent(float angle) {
    const float quadrant = rs::round_ties_even(angle * from_bits(0x3F22F983u));
    float reduced = mul_add(-quadrant, from_bits(0x3FC90FDBu), angle);
    reduced = mul_add(-quadrant, from_bits(0x2E85A309u), reduced);
    const float squared = reduced * reduced;
    float denominator = mul_add(squared, from_bits(0x3505BBA8u), from_bits(0xB9A37B25u));
    denominator = mul_add(squared, denominator, from_bits(0x3CD23CF5u));
    denominator = mul_add(squared, denominator, from_bits(0xBEEEF582u));
    denominator = mul_add(squared, denominator, 1.0f);
    float numerator = mul_add(squared, from_bits(0xB795D5B9u), from_bits(0x3B607415u));
    numerator = mul_add(squared, numerator, from_bits(0xBE0895AFu));
    numerator = mul_add(reduced, squared * numerator, reduced);
    //vcmpbfp's two high result bits select corresponding sign/exponent bits.
    //The bounded constructor argument preserves these bits in both operands;
    //retain the actual bit-select rather than replacing it by a boolean branch.
    const float bound = from_bits(0x39800000u);
    const std::uint32_t mask = (reduced > bound ? 0x80000000u : 0u) | (reduced < -bound ? 0x40000000u : 0u);
    denominator = from_bits((rs::to_bits(denominator) & ~mask) | (rs::to_bits(1.0f) & mask));
    numerator = from_bits((rs::to_bits(numerator) & ~mask) | (rs::to_bits(reduced) & mask));
    if ((rs::as_i32(rs::abs(quadrant)) & 1) == 0) {
        return numerator * reciprocal(denominator, 2);
    }
    return denominator * reciprocal(-numerator, 2);
}

}  // namespace s3::animation::foot_ik::post_contact
