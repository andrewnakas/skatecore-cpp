// Port of skate-game/src/physics/grind/tip_nudge.rs.
#include "s3/game/physics/grind/tip_nudge.hpp"

#include "s3/core/trigonometry.hpp"
#include "s3/game/physics/grind/arithmetic.hpp"
#include "s3/game/physics/grind/board.hpp"

namespace s3::game::physics::grind::tip_nudge {

using arithmetic::dot3;
using V = std::array<float, 4>;

Status apply(::s3::physics::board_runtime::BoardRuntime& board, V position, V point, V across) {
    V offset;
    for (std::size_t i = 0; i < 4; ++i) offset[i] = position[i] - point[i];
    const bool flip = dot3(across, offset) > 0.0f;
    V inward;
    for (std::size_t i = 0; i < 4; ++i) inward[i] = flip ? -across[i] : across[i];
    //82D422A0..C4 multiplies original820996DC=45 by8206D110=deg-to-rad,
    //then RELOADS that product before cosine82473930/sine824531C8.
    //Hex-Rays' stale scalar prints one degree; that is not the loaded value.
    const float angle = 45.0f * rs::from_bits(0x3c8efa35u);
    const auto [sine, cosine] = ::s3::trigonometry::sin_cos(angle);
    //Original v43 combines sine/cosine and v44 is zero. Shift/permutation
    //extract cosine for the lateral multiplier and sine for the Y-only subtract.
    //Do not rotate the full force vector or scale the Y subtract by100.
    V force;
    for (std::size_t i = 0; i < 4; ++i) force[i] = (inward[i] * 100.0f) * cosine;
    force[1] -= sine;
    board::apply_world_force(board, force, position);
    return ok();
}

}  // namespace s3::game::physics::grind::tip_nudge
