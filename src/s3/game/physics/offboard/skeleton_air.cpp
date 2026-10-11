// Port of skate-game/src/physics/offboard/skeleton_air.rs
#include "s3/game/physics/offboard/skeleton_air.hpp"

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/physics/skeleton_biped_air.hpp"
#include "s3/game/physics/animated_skeleton.hpp"
#include "s3/game/physics/animation_input.hpp"
#include "s3/game/physics/skeleton_air.hpp"
#include "s3/game/physics/skeleton_input_runtime.hpp"

namespace s3::game::physics::skeleton_input_runtime {

using offboard::skeleton_ground::ReckoningUpdate;
using sc::physics::skeleton_animation_record::compose_affine;

rs::Result<Transform, std::string> SkeletonInputRuntime::update_biped_air(
    skeleton_air::SkeletonAir& air, sc::physics::board_runtime::BoardRuntime& board,
    offboard::skeleton_air::Input input, ProcessedPhysicsInput& p, SkeletonOwners& owners,
    sc::animation::Slice<const NativeMatrix> globals, const CollisionInput& collision,
    sc::physics::rigid_body::RetailSimulationStep simulation,
    std::function<Status(offboard::skeleton_ground::ReckoningUpdate, const ProcessedPhysicsInput&, float)>
        finish_reckoning) {
    auto& s = owners.animated;
    //82BDEE08 and82BDE310 use the OLD root, before82BDDDC8 changes it.
    s.board_frames.skate_root = compose_affine(s.roots.animation_to_world, s.record.pose[0]);
    s.board_frames.update_com_lift(s.roots.animation_to_world, input.body_target_416, input.lift_436);
    const Transform target =
        sc::physics::skeleton_biped_air::prepare(s.roots, input.frame_208, input.trajectory_position_272,
                                                 s.record.centre_of_mass, drive_frames[0], p.flags_2476, p.flags_2468);
    s.board_frames.animation_target = target;
    if ((p.flags_2480 & 0x8000u) != 0) {
        s.board_frames.physical_board = air.apply_board(board, target, true);
    }
    S3G_CHECK(general_update(p, owners, globals, collision, simulation));
    S3G_CHECK(finish_reckoning(
        ReckoningUpdate{owners.animated.roots.animation_to_world[1], input.board_forward_96, 0.5f}, p,
        owners.animation_input.extra.physical_body_spin));
    owners.animated.finish_ground();
    owners.animation_input.fields.flags2468 = p.flags_2468;
    return target;
}

}  // namespace s3::game::physics::skeleton_input_runtime
