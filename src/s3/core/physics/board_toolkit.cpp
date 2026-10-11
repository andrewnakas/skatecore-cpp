// Port of skate-core/src/physics/board_toolkit.rs.
#include "s3/core/physics/board_toolkit.hpp"

#include "s3/core/physics/board.hpp"
#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/force_queue.hpp"
#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::physics::board_toolkit {

using board::BodyId;
using board_motion_output::inverse_length_squared;
using native_arithmetic::dot3;

BoardToolkit BoardToolkit::from_board(const board_runtime::BoardRuntime& board, std::uint32_t flags_2468,
                                      float speed_2612, V ground_normal_464, V retained_normal) {
    const auto part = board.part_transforms()[board::index(BodyId::Deck)];
    auto axis = [&](std::size_t i) {
        const auto c = part.basis.columns[i];
        return V{c[0], c[1], c[2], 0.0f};
    };
    const M deck{axis(0), axis(1), axis(2), V{part.translation.x, part.translation.y, part.translation.z, 1.0f}};
    std::array<float, board::BODY_COUNT> masses{};
    for (std::size_t i = 0; i < board::BODY_COUNT; ++i) masses[i] = board.bodies()[i].inertia.inverse_mass;
    return calculate(deck, masses, flags_2468, speed_2612, ground_normal_464, retained_normal);
}

BoardToolkit BoardToolkit::calculate(M deck, Slice<const float> inverse_masses, std::uint32_t flags, float speed,
                                     V normal, V retained_normal) {
    const float control_sign = (flags & 0x00100000u) != 0 ? -1.0f : 1.0f;
    M effective = deck;
    for (std::size_t axis : {0u, 2u}) {
        for (std::size_t l = 0; l < 4; ++l) effective[axis][l] = deck[axis][l] * control_sign;
    }
    // 82C0149C..150C transposes XYZ, clears all W lanes, and composes the
    // negative translation in Z,Y,X fused order. Native inverse W is zero.
    M inverse_effective{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        for (std::size_t lane = 0; lane < 3; ++lane) {
            inverse_effective[axis][lane] = effective[lane][axis];
        }
    }
    for (std::size_t lane = 0; lane < 4; ++lane) {
        const float z = (0.0f - effective[3][2]) * inverse_effective[2][lane];
        const float yz = rs::mul_add(0.0f - effective[3][1], inverse_effective[1][lane], z);
        inverse_effective[3][lane] = rs::mul_add(0.0f - effective[3][0], inverse_effective[0][lane], yz);
    }
    const V side = effective[0];
    const V up = deck[1];
    const V forward = effective[2];
    V horizontal_forward = forward;
    horizontal_forward[1] = 0.0f;
    const float horizontal_squared = dot3(horizontal_forward, horizontal_forward);
    if (horizontal_squared > rs::from_bits(0x3727c5acu)) {
        for (float& v : horizontal_forward) v = v * inverse_length_squared(horizontal_squared, 2);
    }
    const float parallel = dot3(up, horizontal_forward);
    V rejected{};
    for (std::size_t i = 0; i < 4; ++i) rejected[i] = up[i] - horizontal_forward[i] * parallel;
    const float inverse = inverse_length_squared(dot3(rejected, rejected), 2);
    V transverse_up{};
    for (std::size_t i = 0; i < 4; ++i) transverse_up[i] = rejected[i] * inverse;
    // The bgt takes only ordered positive values; zero and NaN use -1.
    const float travel_sign = speed > 0.0f ? 1.0f : -1.0f;
    V filtered_normal{};
    if (!(dot3(retained_normal, normal) <= rs::from_bits(0x3f6f5c29u))) {
        filtered_normal = normal;
    } else {
        V blended{};
        for (std::size_t i = 0; i < 4; ++i) {
            blended[i] = rs::mul_add(retained_normal[i], rs::from_bits(0x3f666666u),
                                     normal[i] * rs::from_bits(0x3dcccccdu));
        }
        const float squared = dot3(blended, blended);
        const float inv = inverse_length_squared(squared, 2);
        const float length = squared == 0.0f ? 0.0f : squared * inv;
        if (length > rs::from_bits(0x358637bdu)) {
            for (std::size_t i = 0; i < 4; ++i) filtered_normal[i] = blended[i] * inv;
        }
    }
    BoardToolkit out;
    out.deck = deck;
    out.effective = effective;
    out.inverse_effective = inverse_effective;
    out.side = side;
    out.up = up;
    out.forward = forward;
    out.horizontal_forward = horizontal_forward;
    out.transverse_up = transverse_up;
    for (std::size_t i = 0; i < 4; ++i) {
        out.forward_velocity[i] = forward[i] * speed;
        out.travel_direction[i] = forward[i] * travel_sign;
    }
    out.filtered_normal = filtered_normal;
    out.absolute_speed = speed * travel_sign;
    out.control_sign = control_sign;
    out.total_mass = force_queue::total_body_mass(inverse_masses);
    return out;
}

}  // namespace s3::physics::board_toolkit
