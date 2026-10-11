// Port of skate-game/src/physics/ground_animation.rs.
//! GroundAnimation103 lifecycle82D33BF8/33DB8/33C70/34150, original Skate3TU3.
#include "s3/game/physics/ground_animation.hpp"

#include "s3/core/air/state.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::ground_animation {

void GroundAnimationRuntime::fill(const sc::player::input_phase::ProcessedPhysicsInput& p,
                                  sc::player::input_phase::AirOutputFields& out) const {
    if ((p.flags_2468 & 0x00400000u) != 0) {
        for (std::size_t i = 0; i < 4; ++i) {
            out.jump_velocity_delta_112[i] = rs::to_bits(jump.velocity[i] - rs::from_bits(p.vectors_400_416[0][i]));
        }
    }
    if (launched) {
        out.launched_442 = 1;
        out.launch_velocity_128 = rs::to_bits(launch_velocity);
    }
}

Status enter(GamePhysics& physics, SkaterRuntime& skater) {
    skater.ground_animation.launched = false;
    skater.ground_animation.launch_velocity = {0.0f, 0.0f, 0.0f, 0.0f};
    auto& life = skater.ground_lifecycle;
    life.skeleton_elapsed_16505 = true;
    physics.board.hook_mut().drive.enable_angular_only(life.board_animated_290);
    if (skater.wipeout.state.mode != 1) {
        skater.wipeout.state.mode = 1;
        skater.wipeout.state.balance = 0.0f;
    }
    return ok();
}

Status advance(GamePhysics& physics, SkaterRuntime& skater) {
    skater.ground_animation.launched = false;
    const auto& p = skater.player_input.processed;
    const auto com = rs::from_bits(p.animation_com_to_deck_752);
    const auto heading = physics.riding.reckoning_frames.heading;
    physics.riding.update_ground_reckoning_with_heading(
        physics.board,
        riding_outputs::RidingPoseInputs{
            sc::math::Vector3::new_(com[0], com[1], com[2]),
            skater.animation_input.extra.physical_body_spin,
        },
        p.flags_2468, skater.animation_input.fields.balance, (p.flags_2476 & 0x40000000u) != 0, p, heading);
    S3G_CHECK(skeleton::advance(physics, skater));
    skater.skeleton_output.correction.pending = true;
    return board::advance(physics, skater);
}

Status exit(GamePhysics& physics, SkaterRuntime& skater) {
    board::set_drag(physics, 0.0f);
    physics.settings.wheel_material = physics.settings.standard_wheel_material;
    if (skater.ground_animation.jump.active) {
        const auto& p = skater.player_input.processed;
        auto reference = skater.ground_animation.jump.velocity;
        reference[1] = std::fma(p.gravity_2648, p.timestep_2604, reference[1]);
        const auto velocity = sc::air::state::clamp_jump_velocity(reference, rs::from_bits(p.vectors_400_416[0]));
        skater.ground_runtime.set_animated_velocity(physics.board, velocity);
    }
    skater.ground_animation.jump = sc::air::ground_jump::GroundJump{};
    return ok();
}

}  // namespace s3::game::physics::ground_animation
