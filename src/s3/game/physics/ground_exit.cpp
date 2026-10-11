// Port of skate-game/src/physics/ground_exit.rs.
//!Complete PhysicsGround Exit82D37B20, on the real board and controller owners.
#include "s3/game/physics/ground_exit.hpp"

#include "s3/core/air/state.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::ground_exit {

void exit(GamePhysics& physics, SkaterRuntime& skater) {
    auto& state = skater.ground.state;
    state.steering_damped_turn_2644 = 0.0f;
    state.steering_push_scalar_2640 = 1.0f;
    skater.ground.pumping.reset();
    for (auto& body : physics.board.bodies_mut()) {
        body.inertia.linear_drag = 0.0f;
    }
    physics.settings.wheel_material = physics.settings.standard_wheel_material;
    if (state.flag_2708) {
        const auto& p = skater.player_input.processed;
        const std::array<float, 4> gravity{0.0f, p.gravity_2648, 0.0f, 0.0f};
        std::array<float, 4> reference{};
        for (std::size_t i = 0; i < 4; ++i) reference[i] = std::fma(gravity[i], p.timestep_2604, state.vector_2688[i]);
        const auto velocity = sc::air::state::clamp_jump_velocity(reference, rs::from_bits(p.vectors_400_416[0]));
        skater.ground_runtime.set_animated_velocity(physics.board, velocity);
    }
}

}  // namespace s3::game::physics::ground_exit
