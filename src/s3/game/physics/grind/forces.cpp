// Port of skate-game/src/physics/grind/forces.rs.
#include "s3/game/physics/grind/forces.hpp"

#include "s3/game/physics/grind/board.hpp"

namespace s3::game::physics::grind::forces {

namespace gf = ::s3::physics::grind_forces;
using board::apply_world_force;

///82D3FD88. The family owns its three strengths and position/velocity snapshot.
void friction(BoardRuntime& board, const ManagerObservation& manager, V position, V velocity, V normal,
              std::array<float, 3> strengths) {
    const V force = gf::friction(velocity, normal, manager.geometry.upmost_normal_1408,
                                 manager.surface.friction_vs_time_1496,
                                 (manager.geometry.flags_1476 & 0x40000000u) != 0,
                                 gf::material_multiplier(manager.surface.material_1472), manager.geometry.kind_1464,
                                 strengths);
    // Native skips AddForce for speed<=.001, not an unconditional zero-force
    // call (which would still wake the body). The leaf exposes that separately.
    if (gf::friction_applies(velocity, normal)) {
        apply_world_force(board, force, position);
    }
}

void lateral_pin(BoardRuntime& board, const ManagerObservation& manager, std::array<V, 4> frame, V across, V normal,
                 V velocity, float strength, float forward_offset, float up_offset,
                 const ::s3::point_graph::PointGraph<4>& pin_vs_slope) {
    const float slope = gf::pin_slope(normal, manager.geometry.upmost_normal_1408,
                                      manager.surface.gravity_relief_1512, pin_vs_slope);
    const auto force = gf::lateral_pin(frame, manager.geometry.point_1120, across, velocity, strength, forward_offset,
                                       up_offset, (manager.control.flags_1516 & 0x20000000u) != 0, slope);
    if (force) {
        apply_world_force(board, *force, frame[3]);
    }
}

void slide(BoardRuntime& board, gf::slide::Slide family, gf::slide::Input input) {
    for (const V& force : gf::slide::control(family, input)) {
        apply_world_force(board, force, input.position);
    }
}

void release(BoardRuntime& board, gf::release::Input input) {
    if (const auto force = gf::release::force(input)) {
        apply_world_force(board, *force, input.position);
    }
}

}  // namespace s3::game::physics::grind::forces
