// Port of skate-game/src/physics/plant_skeleton.rs.
//! Skeleton82BDEC48 (anchored foot) and82BDF090 (plant COM).
#include "s3/game/physics/plant_skeleton.hpp"

#include "s3/core/math.hpp"
#include "s3/core/physics/board.hpp"
#include "s3/core/physics/board_animation.hpp"
#include "s3/core/physics/skeleton_air_frames.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/input_phase.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/skeleton_input_runtime.hpp"

namespace s3::game::physics::plant_skeleton {

using sc::math::Vector3;
using sc::physics::board::BodyId;
using sc::physics::board_animation::target_velocity;
using sc::physics::skeleton_air_frames::update_plant_roots;
using sc::physics::skeleton_animation_record::compose_affine;
using sc::physics::skeleton_animation_record::IDENTITY;

Status advance(GamePhysics& physics, SkaterRuntime& skater, std::array<float, 4> anchor,
               std::optional<std::size_t> bone) {
    const auto collision = input_phase::collision(skater);
    auto& p = skater.player_input.processed;
    auto& s = skater.animated_skeleton;
    const std::array<float, 4> local = bone ? skater.skeleton_input.drive_frames[*bone][3] : s.record.centre_of_mass;
    update_plant_roots(s.roots, physics.riding.reckoning_frames.system, anchor, local);
    s.motion.trajectory = IDENTITY;
    s.motion.inverse_trajectory = IDENTITY;
    const auto& local_board = bone.has_value() ? s.record.pose[0] : skater.skeleton_input.drive_frames[0];
    const auto target = compose_affine(s.roots.animation_to_world, local_board);
    p.flags_2468 |= 1u << 19;
    s.board_frames.animation_target = target;
    const auto effective = skater.skeleton_air.apply_board(physics.board, target, true);
    s.board_frames.physical_board = effective;
    s.board_frames.skate_root = effective;
    if (!bone.has_value()) {
        const Vector3 pos = physics.board.bodies()[sc::physics::board::index(BodyId::Deck)].rates.position;
        const auto velocity = target_velocity(effective[3], {pos.x, pos.y, pos.z, 0.0f}, p.timestep_2604);
        for (auto& body : physics.board.bodies_mut()) {
            body.rates.linear_velocity = Vector3::new_(velocity[0], velocity[1], velocity[2]);
        }
    }
    s.board_frames.update_com_lift(s.roots.animation_to_world, s.board_frames.centre_of_mass, 0.0f);
    skeleton_input_runtime::SkeletonOwners owners{
        s,
        skater.skeleton,
        skater.skeleton_drives,
        skater.foot_ik,
        skater.animation_input,
        skater.skeleton_output.correction,
        skater.pose_errors,
    };
    S3G_CHECK(skater.skeleton_input.general_update(p, owners, skater.animation.packet.hierarchy, collision,
                                                   physics.settings.step.simulation));
    owners.animated.finish_ground();
    owners.animation_input.fields.flags2468 = p.flags_2468;
    return ok();
}

void hold_foot(SkaterRuntime& skater, bool right, std::array<float, 4> position, std::uint32_t frames) {
    const std::size_t side = right ? 1 : 0;
    skater.foot_ik.state.external_targets[side].world_position = position;
    skater.foot_ik.state.limbs[side].external_target_set = true;
    skater.foot_ik.state.limbs[side].target_blend = 1.0f;
    if (frames != 0) {
        const std::array<std::size_t, 3> parts =
            right ? std::array<std::size_t, 3>{19, 20, 21} : std::array<std::size_t, 3>{15, 16, 17};
        for (const std::size_t part : parts) {
            skater.skeleton_collision.pending_reenable = true;
            skater.skeleton_collision.disable_count[part] = frames;
            skater.skeleton_collision.parts[part].enabled = false;
        }
    }
}

}  // namespace s3::game::physics::plant_skeleton
