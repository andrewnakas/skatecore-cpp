// Port of skate-game/src/physics/wipeout_states/skeleton.rs
//! Original Skeleton::UpdateWipeout82BDFA88 on the live physical owners.
#include "s3/game/physics/wipeout_states/skeleton.hpp"

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/game/physics/animated_skeleton.hpp"
#include "s3/game/physics/animation_input.hpp"
#include "s3/game/physics/foot_ik.hpp"

namespace s3::game::physics::wipeout_states::skeleton {

namespace drives = ::s3::player::wipeout_state::drives;
namespace core_skeleton = ::s3::player::wipeout_state::skeleton;

///The original r8 byte skips only target-position updates. It does not skip
///root construction, COM frames, first-state-tick IK, or ragdoll drive updates.
rs::Result<float, std::string> update(skeleton_input_runtime::SkeletonInputRuntime& input,
                                      skeleton_input_runtime::SkeletonOwners& owners,
                                      const ::s3::player::input_phase::ProcessedPhysicsInput& processed,
                                      const ::s3::animation::output::NativeMatrix* globals, std::size_t globals_len,
                                      const drives::Settings& settings, float start, float end, float controlled,
                                      float extra, bool skip_targets) {
    auto& s = owners.animated;
    s.roots.initialize_heading = false;
    core_skeleton::update_roots(s.roots, s.animation_hips, owners.body.record.pose[23],
                                owners.body.record.velocities[23], s.board_frames.centre_of_mass,
                                rs::from_bits(processed.vectors_544_560_592_608[3]), processed.state_timer_2664);
    const ::s3::physics::skeleton_body::SkeletonTargetInput target_input{
        s.animation_hips,           s.animation_board,           s.roots.animation_to_world,
        s.roots.inverse_board,      s.board_frames.skate_root,   s.board_frames.com_frame,
        s.board_frames.lifted_com_frame, input.teleporting,
    };
    //82BDFAC8 skips the whole82BE1618 call. Its nested82BE1870 call to
    //82BE18B8 is skipped too: retain all four targets and extra-body velocities.
    if (!skip_targets) {
        input.animation_board_to_physics = owners.drives.targets.update_hook_positions(target_input);
        const auto positions =
            owners.drives.targets.update_extra_targets(owners.body, target_input.com_frame, target_input.lifted_com_frame);
        input.extra_target_positions = {positions.com, positions.lifted_com, positions.following_com};
        owners.pose_errors.set_targets(positions);
    }
    //82BDFA88 ignores the target continuity result: no GeneralUpdate reset.
    s.board_frames.skate_root = s.roots.animation_to_world;
    s.board_frames.update_com_lift(s.roots.animation_to_world, s.board_frames.centre_of_mass, 0.0f);
    if (processed.state_timer_2664 == 0.0f) {
        std::array<std::optional<std::array<float, 4>>, 2> contacts{};
        for (std::size_t i = 0; i < 2; ++i) {
            const auto query = processed.line_tests_960_1008_1056[i];
            if (query.valid != 0) contacts[i] = rs::from_bits(query.position);
        }
        foot_ik::PhysicalInput physical{};
        physical.state_id = processed.state_2508;
        physical.flags_2468 = processed.flags_2468;
        physical.flags_2472 = processed.flags_2472;
        physical.flags_2480 = processed.flags_2480;
        physical.contact_bone = std::size_t(owners.animation_input.contacts.bone);
        physical.physical_board = &s.board_frames.physical_board;
        physical.hips_world_position = owners.body.part_transforms()[23][3];
        physical.current_contacts = contacts;
        S3G_TRY(auto result,
                owners.ik.update(s, sc::animation::Slice<const ::s3::physics::skeleton_animation_record::AnimationPartTransform>(
                                        globals, globals_len),
                                 physical));
        input.drive_frames = result.frames;
    }
    //82BDFB84 passes Skeleton+12624, not the solved physical pose at8016.
    //ProcessData rewrites this animation-volume buffer each tick; first-tick
    //IK may modify it above.82BEB0C8 builds its inverses from that supplied
    //buffer (r4+64..), so physical parts cannot substitute for the drive target.
    drives::Weights weights{};
    weights.start = start;
    weights.end = end;
    weights.controlled = controlled;
    weights.upper_extra = (processed.flags_2484 & 0x20u) != 0 ? extra : 0.0f;
    weights.lower_extra = (processed.flags_2484 & 0x10u) != 0 ? extra : 0.0f;
    const float residual = drives::update(owners.drives, input.drive_frames, settings, weights);
    s.motion.next_trajectory = ::s3::physics::skeleton_animation_record::IDENTITY;
    return residual;
}

}  // namespace s3::game::physics::wipeout_states::skeleton
