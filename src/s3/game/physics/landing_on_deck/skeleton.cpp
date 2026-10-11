// Port of skate-game/src/physics/landing_on_deck/skeleton.rs
//! Skeleton82BDE428: old-root COM, new root, shared drives, shared reckoning.
#include "s3/game/physics/landing_on_deck/skeleton.hpp"

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/physics/skeleton_landing_on_board.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/input_phase.hpp"
#include "s3/game/physics/offboard/skeleton_ground.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/skeleton_input_runtime.hpp"

namespace s3::game::physics::landing_on_deck::skeleton {

using ::s3::physics::skeleton_animation_record::compose_affine;
using ::s3::physics::skeleton_animation_record::IDENTITY;
namespace native = ::s3::physics::skeleton_landing_on_board;

Status update(GamePhysics& physics, SkaterRuntime& skater, std::array<float, 4> position) {
    auto& p = skater.player_input.processed;
    S3G_TRY_OPT(const auto* toolkit, skater.player_input.toolkit ? &*skater.player_input.toolkit : nullptr,
                "Landing skeleton requires the completed board toolkit");
    const auto board = toolkit->deck;
    auto& s = skater.animated_skeleton;
    const auto old = s.roots.animation_to_world;
    s.roots.initialize_heading = true;
    s.board_frames.skate_root =
        native::skate_root(old, skater.skeleton.record.centre_of_mass, p.flags_2480,
                           rs::from_bits(p.vectors_880_896_912_928_944[0][1]), skater.landing_on_deck.root_settings);
    s.board_frames.update_com_lift(old, s.board_frames.centre_of_mass, 0.0f);
    s.motion.trajectory = IDENTITY;
    //82BE090C..0950 resets only packet bone0; inverse trajectory12112 survives.
    auto& packet = skater.animation.packet;
    if (packet.hierarchy.empty()) return rs::make_err(std::string("Landing packet has no trajectory bone"));
    packet.hierarchy.front() = IDENTITY;
    if (packet.local.empty()) return rs::make_err(std::string("Landing packet has no local trajectory bone"));
    packet.local.front() = IDENTITY;
    std::optional<std::uint32_t> revert;
    if ((packet.flags & 0x10000000u) != 0) revert = static_cast<std::uint32_t>(packet.air_dismount_revert_frames);
    native::update_root(s.roots, position, s.record.centre_of_mass, skater.landing_on_deck.state.applied_spin, revert,
                        (p.flags_2476 & 4u) != 0);
    const auto target = compose_affine(s.roots.animation_to_world, skater.skeleton_input.drive_frames[0]);
    s.board_frames.animation_target = target;
    p.flags_2468 |= 0x80000u;
    if ((p.flags_2480 & 0x8000u) != 0) {
        s.board_frames.physical_board = skater.skeleton_air.apply_board(physics.board, target, true);
    }
    const auto collision = input_phase::collision(skater);
    skeleton_input_runtime::SkeletonOwners owners{
        skater.animated_skeleton, skater.skeleton,   skater.skeleton_drives,         skater.foot_ik,
        skater.animation_input,   skater.skeleton_output.correction, skater.pose_errors,
    };
    S3G_CHECK(skater.skeleton_input.general_update(skater.player_input.processed, owners,
                                                   skater.animation.packet.hierarchy, collision,
                                                   physics.settings.step.simulation));
    (void)skater.biped_ground.finish_reckoning(
        offboard::skeleton_ground::ReckoningUpdate{owners.animated.roots.animation_to_world[1], board[2], 0.5f},
        physics.riding, skater.air_reckoning, skater.player_input.processed,
        owners.animation_input.extra.physical_body_spin);
    owners.animated.finish_ground();
    owners.animation_input.fields.flags2468 = skater.player_input.processed.flags_2468;
    return ok();
}

}  // namespace s3::game::physics::landing_on_deck::skeleton
