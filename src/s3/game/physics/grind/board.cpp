// Port of skate-game/src/physics/grind/board.rs.
#include "s3/game/physics/grind/board.hpp"

#include "s3/core/math.hpp"
#include "s3/core/physics/board.hpp"
#include "s3/core/physics/point_force.hpp"

namespace s3::game::physics::grind::board {

using ::s3::math::Basis3;
using ::s3::math::Vector3;
using ::s3::physics::point_force::accumulate_point_force;
using ::s3::physics::point_force::RetailForceAccumulator;

/// Immediate deck acceleration/torque accumulation. Unlike queued board forces,
/// this has no capacity, attribution tag or stock force-point Y offset.
void apply_world_force(::s3::physics::board_runtime::BoardRuntime& board, std::array<float, 4> force,
                       std::array<float, 4> point) {
    auto& deck = board.bodies_mut()[::s3::physics::board::index(::s3::physics::board::BodyId::Deck)];
    apply_to_deck(deck, force, point);
}

void apply_to_deck(::s3::physics::assembly::BodySnapshot& deck, std::array<float, 4> force,
                   std::array<float, 4> point) {
    auto& rates = deck.rates;
    const Vector3 arm = Vector3::new_(point[0] - rates.position.x, point[1] - rates.position.y,
                                      point[2] - rates.position.z);
    // The shared helper's input is already world-space here, so its basis is
    // identity. Preserve its native cross/tensor accumulation order.
    const auto next = accumulate_point_force(
        RetailForceAccumulator{rates.force_acceleration, rates.torque_acceleration, rates.cool_down},
        Vector3::new_(force[0], force[1], force[2]), arm,
        Basis3{{{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}}}, deck.inertia.inverse_mass,
        rates.world_inverse_inertia);
    rates.force_acceleration = next.force_acceleration;
    rates.torque_acceleration = next.torque_acceleration;
    rates.cool_down = next.cool_down;
}

}  // namespace s3::game::physics::grind::board
