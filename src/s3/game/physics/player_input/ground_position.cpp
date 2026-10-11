// Port of skate-game/src/physics/player_input/ground_position.rs.
//! Ground-position observation from TU3 Skateboard::CalculateGroundPosition
//! 82C02840. The four wheel rigid-body centres are expressed in Reckoning752,
//! averaged laterally/longitudinally, and placed at the lowest wheel centre.
#include "s3/game/physics/player_input/ground_position.hpp"

namespace s3::game::physics::player_input::ground_position {

std::array<std::uint32_t, 4> ground_position(const sc::physics::board_runtime::BoardRuntime& board,
                                             const AnimationPartTransform& ground) {
    std::array<std::array<float, 4>, 4> wheels{};
    for (std::size_t i = 0; i < 4; ++i) {
        const auto p = board.bodies()[i].rates.position;
        wheels[i] = {p.x, p.y, p.z, 0.0f};
    }
    return rs::to_bits(wheel_ground_position(wheels, ground));
}

std::array<float, 4> wheel_ground_position(std::array<std::array<float, 4>, 4> wheels,
                                           const AnimationPartTransform& ground) {
    // Native transpose clears its fourth lane; the source does not subtract
    // or re-add the frame translation in this calculation.
    std::array<std::array<float, 4>, 3> transpose{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        transpose[axis] = {ground[0][axis], ground[1][axis], ground[2][axis], 0.0f};
    }
    std::array<std::array<float, 4>, 4> local{};
    for (std::size_t w = 0; w < 4; ++w) local[w] = rotate(transpose, wheels[w]);
    std::array<float, 4> mean{};
    for (std::size_t i = 0; i < 4; ++i) mean[i] = (((local[0][i] + local[1][i]) + local[2][i]) + local[3][i]) * 0.25f;
    const float first_minimum = local[1][1] > local[0][1] ? local[0][1] : local[1][1];
    // fsel chooses its negative input on an unordered comparison as well.
    const float next_minimum = local[2][1] - first_minimum >= 0.0f ? first_minimum : local[2][1];
    mean[1] = local[3][1] - next_minimum >= 0.0f ? next_minimum : local[3][1];
    return rotate({ground[0], ground[1], ground[2]}, mean);
}

std::array<float, 4> rotate(const std::array<std::array<float, 4>, 3>& basis, std::array<float, 4> point) {
    std::array<float, 4> out{};
    for (std::size_t i = 0; i < 4; ++i) {
        const float x = basis[0][i] * point[0];
        const float xy = rs::mul_add(basis[1][i], point[1], x);
        out[i] = rs::mul_add(basis[2][i], point[2], xy);
    }
    return out;
}

}  // namespace s3::game::physics::player_input::ground_position
