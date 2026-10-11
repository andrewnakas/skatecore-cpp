// Port of skate-game/src/physics/offboard/post_physics.rs
#include "s3/game/physics/offboard/post_physics.hpp"

#include "s3/game/physics.hpp"
#include "s3/game/physics/biped_air.hpp"
#include "s3/game/physics/biped_ground.hpp"
#include "s3/game/physics/landing_on_deck.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/solve.hpp"
#include "s3/game/physics/wipeout.hpp"

namespace s3::game::physics::offboard::post_physics {

using sc::player::state::PhysicalStateId;

Status advance(const GamePhysics& physics, SkaterRuntime& skater) {
    const PhysicalStateId state = skater.player_state.current();
    if (!(state == PhysicalStateId::BipedGround || state == PhysicalStateId::OffBoardPushing ||
          state == PhysicalStateId::BipedAir || state == PhysicalStateId::LandingOnDeck)) {
        return ok();
    }
    //Borrow the already completed reports and pose errors. This produces no
    //contacts, default support, replacement dynamics, or second request owner.
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
    S3G_TRY(auto frame, observations.frame());
    switch (state) {
        case PhysicalStateId::LandingOnDeck: return landing_on_deck::post(skater, frame);
        case PhysicalStateId::BipedAir: return biped_air::post::post(skater, frame);
        case PhysicalStateId::BipedGround:
        case PhysicalStateId::OffBoardPushing: {
            sc::player::offboard::ground_lifecycle::CollisionInput collision{};
            collision.shared = &frame;
            collision.skeleton_velocity_16336 = skater.skeleton_input.root_velocity;
            collision.contact_flag_4072 = skater.collision_feedback.flags.group_8;
            collision.contact_force_4056 = skater.collision_feedback.maximum_group_8_force;
            biped_ground::post(skater, collision);
            return ok();
        }
        default: S3_PANIC("offboard state checked before borrowing its observations");
    }
}

}  // namespace s3::game::physics::offboard::post_physics
