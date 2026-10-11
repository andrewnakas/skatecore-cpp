// Port of skate-game/src/physics/grind/phase.rs.
#include "s3/game/physics/grind/phase.hpp"

#include "s3/game/physics.hpp"
#include "s3/game/physics/grind.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/solve.hpp"
#include "s3/game/physics/wipeout.hpp"

namespace s3::game::physics::grind::phase {

Status enter(GamePhysics& physics, SkaterRuntime& skater) {
    if (static_cast<std::uint32_t>(skater.player_state.current()) == 701) {
        // Original82D42D80: no feet/drag changes; reset the shared spin only here.
        skater.grind.nonspecific_active = true;
        skater.grind.nonspecific_jumped = false;
        skater.grind.nonspecific_jump_velocity = {0.0f, 0.0f, 0.0f, 0.0f};
        skater.ground_lifecycle.skeleton_elapsed_16505 = true;
        skater.air_reckoning.state.spin_angle = 0.0f;
        skater.air_reckoning.state.spin_speed = 0.0f;
        physics.board.hook_mut().drive.disable_animation(skater.ground_lifecycle.board_animated_290);
        return ok();
    }
    S3G_TRY_OPT(const Family family,
                from_physical_state(static_cast<std::uint32_t>(skater.player_state.current())),
                "Grind Enter requires selected state400..405");
    if (skater.grind.active) {
        return rs::make_err(std::string("Grind Enter requires prior Exit"));
    }
    if (!skater.player_input.toolkit) {
        return rs::make_err(std::string("Grind Enter requires current board toolkit"));
    }
    const auto frame = skater.player_input.toolkit->deck;
    lifecycle::enter(physics.board, skater.ground_lifecycle.board_animated_290,
                     skater.ground_lifecycle.skeleton_elapsed_16505, skater.skeleton_collision);
    skater.grind.states[index(family)].enter(family, frame);
    skater.grind.active = family;
    return ok();
}

Status exit(GamePhysics& physics, SkaterRuntime& skater) {
    if (skater.grind.nonspecific_active) {
        // Vtable8232861C+20 is82B61BB8 (empty), not common grind Exit.
        skater.grind.nonspecific_active = false;
        return ok();
    }
    S3G_TRY_OPT(const Family family, skater.grind.active, "Grind Exit requires active physical state");
    std::array<bool, 4> collisions{};
    const std::array<std::size_t, 4> bones = {19, 15, 20, 16};
    for (std::size_t k = 0; k < 4; ++k) {
        collisions[k] = skater.skeleton.definition.bones[bones[k]].has_collision;
    }
    lifecycle::exit(physics.board, skater.ground_lifecycle.board_animated_290, skater.skeleton_collision, collisions,
                    skater.grind.settings.standard_angular_drag);
    // Common Exit82D3F448 clears the retained leaving byte96.
    skater.grind.states[index(family)].output.leaving = false;
    skater.grind.active = std::nullopt;
    // Camera/chromosome history is updated by the ordinary output phase,
    // not reset by a physical-state transition.
    return ok();
}

Status fill(const GamePhysics& /*physics*/, SkaterRuntime& skater) {
    if (skater.grind.nonspecific_active) {
        // Original82D430A0 only writes Air442/128 on this retained latch.
        if (skater.grind.nonspecific_jumped) {
            skater.player_input.physical.air.launch_velocity_128 = rs::to_bits(skater.grind.nonspecific_jump_velocity);
            skater.player_input.physical.air.launched_442 = 1;
        }
        return ok();
    }
    S3G_TRY_OPT(const ManagerObservation manager, skater.grind.manager,
                "Grind Fill requires completed manager observation");
    S3G_TRY_OPT(const auto side_effects, skater.grind.fill_physical(manager, skater.player_input.physical.grinds),
                "Grind Fill requires active state");
    if (side_effects.air_jump_velocity_128) {
        skater.player_input.physical.air.launch_velocity_128 = rs::to_bits(*side_effects.air_jump_velocity_128);
        skater.player_input.physical.air.launched_442 = 1;
    }
    skater.grind.pending_wipeout_impulse = side_effects.wipeout_impulse_16;
    wipeout_output::publish(skater);
    return ok();
}

Status post(const GamePhysics& physics, SkaterRuntime& skater) {
    const wipeout::Observations observations{
        skater.player_input.processed,
        physics.riding.ground,
        skater.collision_feedback,
        solve::deck_frame(physics.board),
        skater.animated_skeleton.board_frames.animation_target,
        skater.animated_skeleton.roots.world_to_animation,
        skater.collision_pose_error,
        skater.collision_maximum_error,
        skater.player_state.post.jump_fix_frames,
        skater.air_reckoning.state,
        physics.riding.reckoning_frames.system[1][1],
        skater.trajectory.selector.grind_locked_to_middle(),
        skater.trajectory.selector.grind_normal(),
    };
    return post_::check(skater.wipeout.state, skater.grind.settings.post, observations);
}

}  // namespace s3::game::physics::grind::phase
