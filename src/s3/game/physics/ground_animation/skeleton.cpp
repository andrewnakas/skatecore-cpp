// Port of skate-game/src/physics/ground_animation/skeleton.rs.
//!82D33E00 calls SkeletonUpdateAnimated82BDDA10 with fast blending enabled.
#include "s3/game/physics/ground_animation/skeleton.hpp"

#include "s3/game/physics.hpp"
#include "s3/game/physics/input_phase.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/skeleton_input_runtime.hpp"

namespace s3::game::physics::ground_animation::skeleton {

using skeleton_input_runtime::SkeletonOwners;

Status advance(GamePhysics& physics, SkaterRuntime& skater) {
    const auto collision = input_phase::collision(skater);
    SkeletonOwners owners{
        skater.animated_skeleton, skater.skeleton,        skater.skeleton_drives,
        skater.foot_ik,           skater.animation_input, skater.skeleton_output.correction,
        skater.pose_errors,
    };
    S3G_CHECK(skater.skeleton_input.update_animated(skater.skeleton_air, physics.board,
                                                    physics.riding.reckoning_frames.system,
                                                    skater.player_input.processed, owners,
                                                    skater.animation.packet.hierarchy, collision,
                                                    physics.settings.step.simulation, true));
    return ok();
}

}  // namespace s3::game::physics::ground_animation::skeleton
