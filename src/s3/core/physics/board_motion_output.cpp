// Port of skate-core/src/physics/board_motion_output.rs.
#include "s3/core/physics/board_motion_output.hpp"

#include "s3/core/physics/board.hpp"
#include "s3/core/physics/board_runtime.hpp"

namespace s3::physics::board_motion_output {

using board::BodyId;

BoardMotionOutput BoardMotionOutput::from_board(const board_runtime::BoardRuntime& board,
                                                Vector3 reckoning_ground_normal,
                                                std::uint32_t processed_flags_2468) {
    const auto deck = board.bodies()[board::index(BodyId::Deck)].rates;
    Basis3 effective_basis = board.part_transforms()[board::index(BodyId::Deck)].basis;
    // Complete GetEffectiveTransform82C01BF8 axis adjustment; translation
    // and Y remain the physical part's values.
    if ((processed_flags_2468 & 0x00100000u) != 0) {
        for (std::size_t axis : {0u, 2u}) {
            for (float& v : effective_basis.columns[axis]) v = -v;
        }
    }
    const float normal_speed = dot(deck.linear_velocity, reckoning_ground_normal);
    const Vector3 ground_velocity = subtract(deck.linear_velocity, scale(reckoning_ground_normal, normal_speed));
    const auto z = effective_basis.columns[2];
    BoardMotionOutput out;
    out.angular_velocity = deck.angular_velocity;
    out.linear_velocity = deck.linear_velocity;
    out.ground_velocity = ground_velocity;
    out.speed = length(deck.linear_velocity);
    out.ground_speed = length(ground_velocity);
    out.forward_speed = dot(deck.linear_velocity, Vector3{z[0], z[1], z[2]});
    out.effective_basis = effective_basis;
    return out;
}

}  // namespace s3::physics::board_motion_output
