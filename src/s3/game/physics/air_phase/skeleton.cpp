// Port of skate-game/src/physics/air_phase/skeleton.rs.
//! Same live Skeleton owners as Ground; Air chooses its original update method.
#include "s3/game/physics/air_phase/skeleton.hpp"

#include "s3/game/physics.hpp"
#include "s3/game/physics/input_phase.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/skeleton_input_runtime.hpp"

namespace s3::game::physics::air_phase::skeleton {

using skeleton_input_runtime::SkeletonOwners;

Status advance(GamePhysics& physics, SkaterRuntime& skater, std::optional<std::array<float, 4>> com) {
    const auto collision = input_phase::collision(skater);
    SkeletonOwners owners{
        skater.animated_skeleton, skater.skeleton,        skater.skeleton_drives,
        skater.foot_ik,           skater.animation_input, skater.skeleton_output.correction,
        skater.pose_errors,
    };
    const auto& packet = skater.animation.packet;
    if (com) {
        S3G_CHECK(skater.skeleton_input.update_known_air(skater.skeleton_air, physics.board,
                                                         physics.riding.reckoning_frames.system, *com, packet,
                                                         skater.player_input.processed, owners, packet.hierarchy,
                                                         collision, physics.settings.step.simulation));
    } else {
        S3G_CHECK(skater.skeleton_input.update_animated(skater.skeleton_air, physics.board,
                                                        physics.riding.reckoning_frames.system,
                                                        skater.player_input.processed, owners, packet.hierarchy,
                                                        collision, physics.settings.step.simulation, false));
    }
    return ok();
}

}  // namespace s3::game::physics::air_phase::skeleton
