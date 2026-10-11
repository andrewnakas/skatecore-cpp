// Port of skate-game/src/physics/ground_runtime/launch.rs.
//! Wall-jump launch packet;82D33448 constructs it,82BE33D0 fills it.
//! The native selector consumes this packet after body velocity publication.
#include "s3/game/physics/ground_runtime/launch.hpp"

namespace s3::game::physics::ground_runtime::launch {

sc::air::trajectory::types::LaunchInfo GroundLaunchInfo::selector_launch() const {
    sc::air::trajectory::types::LaunchInfo l;
    l.reckoning_transform = system;
    l.reckoning_inverse = inverse_system;
    l.start_velocity = velocity;
    l.com_velocity = angular_velocity;
    l.skeleton_vector_160 = skeleton_vector_16208;
    l.skeleton_vector_176 = skeleton_vector_16240;
    l.board_position = board_position;
    l.animation_com_position = physical_center_of_mass;
    l.start_position_override = vector_224;
    l.board_position_override = vector_240;
    l.cone_angle_x = cone_angle_x;
    l.cone_angle_z = cone_angle_z;
    l.timestep = time_step;
    l.player_jumped = wall_jump_;
    l.use_position_override = flag_269;
    l.trajectory_count = flags_270;
    return l;
}

void GroundLaunchInfo::fill(const GroundLaunchPhysical& p, float cone_angle_x_, float cone_angle_z_) {
    board_position = p.board_position;
    physical_center_of_mass = p.physical_center_of_mass;
    skeleton_vector_16208 = p.skeleton_vector_16208;
    skeleton_vector_16240 = p.skeleton_vector_16240;
    system = p.reckoning.system;
    inverse_system = p.reckoning.inverse_system;
    velocity = p.velocity;
    angular_velocity = p.angular_velocity;
    time_step = p.time_step;
    flags_270 = (p.flags_2468 & 0x2000u) != 0 ? 7 : 1;
    cone_angle_x = cone_angle_x_;
    cone_angle_z = cone_angle_z_;
}

void GroundLaunchInfo::wall_jump(std::array<float, 4> velocity_) {
    velocity = velocity_;
    angular_velocity = velocity_;
    wall_jump_ = true;
}

}  // namespace s3::game::physics::ground_runtime::launch
