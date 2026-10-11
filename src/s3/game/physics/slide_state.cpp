// Port of skate-game/src/physics/slide_state.rs.
//! PhysicsSlideGround101 production lifecycle, original TU3 vtable823285B4.
#include "s3/game/physics/slide_state.hpp"

#include "s3/game/physics.hpp"
#include "s3/game/physics/input_phase.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::slide_state {

Status enter(GamePhysics& physics, SkaterRuntime& skater) {
    const auto& p = skater.player_input.processed;
    skater.ground_lifecycle.skeleton_elapsed_16505 = true;
    skater.air_reckoning.state.spin_angle = 0.0f;
    skater.air_reckoning.state.spin_speed = 0.0f;
    physics.board.hook_mut().drive.disable_animation(skater.ground_lifecycle.board_animated_290);
    physics.board.bodies_mut()[6].rates.angular_velocity = sc::math::Vector3::ZERO;
    for (auto& body : physics.board.bodies_mut()) {
        body.inertia.linear_drag = 0.0f;
    }
    if (p.category_2516 != 100) {
        skater.ground.manual.reset();
    }
    if (skater.wipeout.state.mode != 1) {
        skater.wipeout.state.balance = 0.0f;
        skater.wipeout.state.mode = 1;
    }
    skater.slide_state.state.enter(p.scalar_2656);
    return ok();
}

Status exit(GamePhysics& physics, SkaterRuntime& skater) {
    skater.slide_state.state.exit();
    physics.settings.wheel_material = physics.settings.standard_wheel_material;
    return ok();
}

Status update(GamePhysics& physics, SkaterRuntime& skater) {
    const auto& p = skater.player_input.processed;
    if (!skater.player_input.toolkit) {
        return rs::make_err(std::string("Slide requires current BoardToolkit"));
    }
    const auto& t = *skater.player_input.toolkit;
    physics.riding.update_slide_reckoning(p, t, skater.animation_input.extra.physical_body_spin,
                                          skater.animation_input.fields.balance);
    S3G_CHECK(input_phase::update_ground(physics, skater));
    return update_::board(physics, skater);
}

}  // namespace s3::game::physics::slide_state
