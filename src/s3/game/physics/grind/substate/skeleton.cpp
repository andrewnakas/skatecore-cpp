// Port of skate-game/src/physics/grind/substate/skeleton.rs.
#include "s3/game/physics/grind/substate/skeleton.hpp"

#include "s3/game/physics.hpp"
#include "s3/game/physics/input_phase.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/skeleton_input_runtime.hpp"

namespace s3::game::physics::grind::substate::skeleton {

using skeleton_input_runtime::SkeletonOwners;

Status animated(GamePhysics& physics, SkaterRuntime& skater) {
    S3G_CHECK(animated_target(physics, skater));
    return ok();
}

rs::Result<AnimationPartTransform, std::string> animated_target(GamePhysics& physics, SkaterRuntime& skater) {
    const auto collision = input_phase::collision(skater);
    SkeletonOwners owners{
        skater.animated_skeleton, skater.skeleton,        skater.skeleton_drives,
        skater.foot_ik,           skater.animation_input, skater.skeleton_output.correction,
        skater.pose_errors,
    };
    return skater.skeleton_input.update_animated(skater.skeleton_air, physics.board,
                                                 physics.riding.reckoning_frames.system,
                                                 skater.player_input.processed, owners,
                                                 skater.animation.packet.hierarchy, collision,
                                                 physics.settings.step.simulation, false);
}

///Nonspecific performs82C04368 in its subsequent board helper, after steering.
rs::Result<AnimationPartTransform, std::string> ground_target(const GamePhysics& physics, SkaterRuntime& skater) {
    const auto collision = input_phase::collision(skater);
    SkeletonOwners owners{
        skater.animated_skeleton, skater.skeleton,        skater.skeleton_drives,
        skater.foot_ik,           skater.animation_input, skater.skeleton_output.correction,
        skater.pose_errors,
    };
    return skater.skeleton_input.update_ground(physics.board, physics.riding.reckoning_frames.system,
                                               skater.player_input.processed, owners,
                                               skater.animation.packet.hierarchy, collision,
                                               physics.settings.step.simulation);
}

}  // namespace s3::game::physics::grind::substate::skeleton
