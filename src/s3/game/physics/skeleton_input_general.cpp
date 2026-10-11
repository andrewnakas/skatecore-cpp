// Port of skate-game/src/physics/skeleton_input_general.rs.
#include "s3/game/physics/skeleton_input_general.hpp"

#include "s3/core/physics/drive_parameters.hpp"
#include "s3/core/physics/skeleton_general.hpp"
#include "s3/game/physics/animated_skeleton.hpp"
#include "s3/game/physics/animation_input.hpp"
#include "s3/game/physics/skeleton_input_runtime.hpp"

namespace s3::game::physics::skeleton_input_runtime {

namespace skeleton_general = sc::physics::skeleton_general;

rs::Result<Transform, std::string> SkeletonInputRuntime::update_ground(
    const sc::physics::board_runtime::BoardRuntime& board, const Transform& reckoning, ProcessedPhysicsInput& p,
    SkeletonOwners& owners, sc::animation::Slice<const NativeMatrix> globals, const CollisionInput& collision,
    sc::physics::rigid_body::RetailSimulationStep simulation) {
    const Transform frame = owners.animated.prepare_ground(board, reckoning, p.timestep_2604, p.flags_2468);
    S3G_CHECK(general_update(p, owners, globals, collision, simulation));
    owners.animated.finish_ground();
    owners.animation_input.fields.flags2468 = p.flags_2468;
    return frame;
}

rs::Result<foot_ik::DriveOutput, std::string> SkeletonInputRuntime::general_update(
    const ProcessedPhysicsInput& p, SkeletonOwners& owners, sc::animation::Slice<const NativeMatrix> globals,
    const CollisionInput& collision, sc::physics::rigid_body::RetailSimulationStep simulation) {
    root_velocity = skeleton_general::root_velocity(previous_root_position,
                                                    owners.animated.roots.animation_to_world[3], p.timestep_2604);
    const auto& s = owners.animated;
    const sc::physics::skeleton_body::target_update::SkeletonTargetInput target_input{
        s.animation_hips,         s.animation_board,           s.roots.animation_to_world,
        s.roots.inverse_board,    s.board_frames.skate_root,   s.board_frames.com_frame,
        s.board_frames.lifted_com_frame, teleporting,
    };
    const auto target = owners.drives.targets.update_positions(target_input, owners.body);
    animation_board_to_physics = target.animation_board_to_physics;
    extra_target_positions = {target.positions.com, target.positions.lifted_com, target.positions.following_com};
    owners.pose_errors.set_targets(target.positions);
    if (!target.continuous) {
        reset_physical_pose(owners, simulation);
        if (p.state_2508 != 0 && p.state_2508 != 702) invalid_target_reset = true;
    }
    std::array<std::optional<std::array<float, 4>>, 2> contacts{};
    for (std::size_t i = 0; i < 2; ++i) {
        const auto& query = p.line_tests_960_1008_1056[i];
        if (query.valid != 0) contacts[i] = rs::from_bits(query.position);
    }
    const std::array<float, 4> physical_hips = owners.body.part_transforms()[23][3];
    foot_ik::PhysicalInput input;
    input.state_id = p.state_2508;
    input.flags_2468 = p.flags_2468;
    input.flags_2472 = p.flags_2472;
    input.flags_2480 = p.flags_2480;
    //Unknown event bone -1 cannot equal either real toe index.
    input.contact_bone = static_cast<std::size_t>(owners.animation_input.contacts.bone);
    input.physical_board = &owners.animated.board_frames.physical_board;
    input.hips_world_position = physical_hips;
    input.current_contacts = contacts;
    S3G_TRY(foot_ik::DriveOutput drives, owners.ik.update(owners.animated, globals, input));
    drive_frames = drives.frames;
    owners.drives.update(drives.frames, collision.partial_ragdoll, collision.drive_weight_4028);
    skeleton_general::apply_gravity_displacement(owners.body, p.gravity_2648);
    return drives;
}

void SkeletonInputRuntime::reset_physical_pose(SkeletonOwners& owners,
                                               sc::physics::rigid_body::RetailSimulationStep simulation) {
    //82BE27B0 clears the same16388 that states arm and PostWipeoutCheck
    //consumes. A discontinuity must not retain a detached lifecycle copy.
    owners.correction.pending = false;
    auto& s = owners.animated;
    const auto observation = skeleton_general::reset_to_animation(owners.body, s.record, drive_frames,
                                                                  s.roots.animation_to_world,
                                                                  s.board_frames.com_frame, simulation);
    s.board_frames.centre_of_mass = observation.world_hips;
    s.board_frames.previous_centre_of_mass = observation.world_hips;
    s.board_frames.com_velocity = {0.0f, 0.0f, 0.0f, 0.0f};
    reset_local_hips = observation.local_hips;
    owners.pose_errors.reset_history();
}

namespace general {

void restore_board_target_drives(::s3::physics::skeleton_body::SkeletonDrives& drives) {
    ::s3::physics::drive_parameters::RetailDriveParams hard;
    hard.spring_or_max_velocity = rs::from_bits(0x4415FFFFu);
    hard.damping = 0.0f;
    hard.max_strength = rs::from_bits(0x470C9FFFu);
    hard.drive_type = ::s3::physics::drive_parameters::RetailDriveType::HardDrive;
    drives.targets.dynamics[0].linear = hard;
    drives.targets.dynamics[0].angular = hard;
}

}  // namespace general

}  // namespace s3::game::physics::skeleton_input_runtime
