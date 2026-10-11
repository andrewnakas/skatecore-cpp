// Port of skate-core/src/physics/skeleton_board_frames.rs.
#include "s3/core/physics/skeleton_board_frames.hpp"

#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::physics::skeleton_board_frames {

using native_arithmetic::reciprocal_estimate;
using skeleton_animation_record::compose_affine;
using skeleton_animation_record::IDENTITY;
using skeleton_animation_record::transform_point;
using skeleton_root::orthonormalize;

void SkeletonBoardFrames::reset(Transform spawn) {
    com_frame = spawn;
    com_frame[3][1] += 1.0f;
    lifted_com_frame = com_frame;
    centre_of_mass = spawn[3];
    local_centre_of_mass = {0.0f, 0.0f, 0.0f, 0.0f};
}

void SkeletonBoardFrames::publish_local_observations(const SkeletonRootFrames& roots,
                                                     const Transform& physical_board_frame) {
    local_centre_of_mass = transform_point(roots.world_to_animation, centre_of_mass);
    local_board_position = transform_point(roots.world_to_animation, physical_board_frame[3]);
}

void SkeletonBoardFrames::publish_centre_of_mass(std::array<float, 4> com, float dt, std::uint32_t flags_2472) {
    previous_centre_of_mass = centre_of_mass;
    centre_of_mass = com;
    float inverse = reciprocal_estimate(dt);
    for (int i = 0; i < 2; ++i) {
        inverse = rs::mul_add(inverse, rs::mul_add(-inverse, dt, 1.0f), inverse);
    }
    for (std::size_t i = 0; i < 4; ++i) com_velocity[i] = (com[i] - previous_centre_of_mass[i]) * inverse;
    if ((flags_2472 & 0x400u) != 0) {
        //Native updates previous COM after calculating velocity.
        previous_centre_of_mass = com;
    }
}

Transform SkeletonBoardFrames::prepare_ground(const SkeletonRootFrames& roots, const Transform& mapped_board,
                                              Transform actual_board, std::uint32_t& flags_2468) {
    const Transform local_target = compose_affine(roots.animation_to_board, mapped_board);
    Transform world_position = IDENTITY;
    world_position[3] = roots.predicted_board_position;
    animation_target = compose_affine(world_position, local_target);
    flags_2468 &= ~0x80000u;
    physical_board = actual_board;
    skate_root = actual_board;
    update_com_lift(roots.animation_to_world, centre_of_mass, 0.0f);
    return animation_target;
}

Transform SkeletonBoardFrames::prepare_teleport(const SkeletonRootFrames& roots, const Transform& mapped_board,
                                                Transform actual_board, std::uint32_t& flags_2468) {
    const Transform local_target = compose_affine(roots.animation_to_board, mapped_board);
    Transform world_position = IDENTITY;
    world_position[3] = roots.predicted_board_position;
    animation_target = compose_affine(world_position, local_target);
    flags_2468 &= ~0x80000u;
    physical_board = actual_board;
    skate_root = actual_board;
    std::array<float, 4> position = actual_board[3];
    position[1] += 1.0f;  //82BDFA20..30 replaces Y only.
    update_com_lift(roots.animation_to_world, position, 0.0f);
    return animation_target;
}

void SkeletonBoardFrames::update_com_lift(const Transform& animation_to_world, std::array<float, 4> position,
                                          float requested_height) {
    const float step = rs::from_bits(0x3D23D70Au);  //82216FEC .04
    const float low = lift_height - step;
    const float high = lift_height + step;
    const float above_low = low - requested_height >= 0.0f ? low : requested_height;
    lift_height = high - above_low >= 0.0f ? above_low : high;
    Transform frame = animation_to_world;
    frame[3] = position;
    com_frame = orthonormalize(frame);
    lifted_com_frame = com_frame;
    const float height = lift_height + rs::from_bits(0x3E99999Au);  //820D06C0 .3
    for (std::size_t i = 0; i < 4; ++i) {
        lifted_com_frame[3][i] = rs::mul_add(com_frame[1][i], height, com_frame[3][i]);
    }
}

}  // namespace s3::physics::skeleton_board_frames
