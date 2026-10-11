// Port of skate-core/src/physics/skeleton_air_frames.rs.
#include "s3/core/physics/skeleton_air_frames.hpp"

#include "s3/core/trigonometry.hpp"

namespace s3::physics::skeleton_air_frames {

using skeleton_animation_record::compose_affine;
using skeleton_animation_record::IDENTITY;
using skeleton_root::inverse_rigid;
using skeleton_root::orthonormalize;

Transform prepare_animated(const SkeletonRootFrames& roots, SkeletonBoardFrames& board, const Transform& mapped_board,
                           std::uint32_t& flags) {
    const Transform local = compose_affine(roots.animation_to_board, mapped_board);
    Transform placement = IDENTITY;
    placement[3] = roots.predicted_board_position;
    const Transform target = compose_affine(placement, local);
    flags |= 1u << 19;
    board.animation_target = target;
    board.skate_root = target;
    board.update_com_lift(roots.animation_to_world, board.centre_of_mass, 0.0f);
    return target;
}

void update_known_air_roots(SkeletonRootFrames& roots, const Transform& reckoning, std::array<float, 4> target_com,
                            std::array<float, 4> animation_com, AirDismountRevert revert) {
    if (roots.initialize_heading) {
        roots.heading_alignment = compose_affine(inverse_rigid(reckoning), roots.animation_to_world);
        //82BDE8EC overwrites the entire affine translation with zero.
        roots.heading_alignment[3] = {0.0f, 0.0f, 0.0f, 0.0f};
        roots.initialize_heading = false;
    }
    // A zero frame count cannot express "pi spread over N frames", and dividing
    // by it produces +inf, which `sin_cos` turns into NaN (its range reduction
    // computes `fma(-2pi, inf, inf)`). That NaN reaches every row of
    // `animation_to_world`, because `world[3]` below is rebuilt from
    // `world[0..2]`, and from there the deck's torque -- the crash reported as
    // "Non-finite torque_acceleration before shared solve".
    //
    // Retail's animation supplies a real frame count alongside the request bit.
    // This engine's does not: `air_dismount_revert_frames` is written only as 0
    // (`skater_animation.rs`, `skater_animation/state.rs`), while the graph does
    // raise the request bit, so the division was guaranteed to be by zero. Until
    // the producer is ported, a revert with no frames is treated as no revert --
    // skipping a rotation is recoverable, poisoning the skeleton root is not.
    if (revert.requested && revert.frames > 0) {
        float angle = rs::from_bits(0x40490fdbu) / static_cast<float>(revert.frames);
        if (!revert.goofy) {
            angle = -angle;
        }
        const auto [sin, cos] = trigonometry::sin_cos(angle);
        const Transform rotation{{
            {cos, 0.0f, -sin, 0.0f},
            {0.0f, 1.0f, 0.0f, 0.0f},
            {sin, 0.0f, cos, 0.0f},
            {0.0f, 0.0f, 0.0f, 0.0f},
        }};
        roots.heading_alignment = orthonormalize(compose_affine(roots.heading_alignment, rotation));
    }
    Transform world = compose_affine(reckoning, roots.heading_alignment);
    //82BDEB98..BB8: rotate COM without adding the prior translation.
    std::array<float, 4> translation{};
    for (std::size_t lane = 0; lane < 4; ++lane) {
        const float x = world[0][lane] * animation_com[0];
        const float y = rs::mul_add(world[1][lane], animation_com[1], x);
        translation[lane] = target_com[lane] - rs::mul_add(world[2][lane], animation_com[2], y);
    }
    world[3] = translation;
    roots.animation_to_world = orthonormalize(world);
    roots.world_to_animation = inverse_rigid(roots.animation_to_world);
}

Transform prepare_known_air(const SkeletonRootFrames& roots, SkeletonBoardFrames& board,
                            const Transform& mapped_board, std::uint32_t& flags) {
    const Transform target = compose_affine(roots.animation_to_world, mapped_board);
    flags |= 1u << 19;
    board.animation_target = target;
    return target;
}

void finish_known_air(SkeletonRootFrames& roots, SkeletonBoardFrames& board, Transform effective_board) {
    board.physical_board = effective_board;
    board.skate_root = effective_board;
    board.update_com_lift(roots.animation_to_world, board.centre_of_mass, 0.0f);
    roots.predicted_board_position = effective_board[3];
    roots.supplied_prediction = effective_board[3];
}

void update_plant_roots(SkeletonRootFrames& roots, const Transform& reckoning, std::array<float, 4> world_anchor,
                        std::array<float, 4> animation_anchor) {
    Transform world = compose_affine(reckoning, roots.heading_alignment);
    std::array<float, 4> translation{};
    for (std::size_t i = 0; i < 4; ++i) {
        const float x = world[0][i] * animation_anchor[0];
        const float y = rs::mul_add(world[1][i], animation_anchor[1], x);
        translation[i] = world_anchor[i] - rs::mul_add(world[2][i], animation_anchor[2], y);
    }
    world[3] = translation;
    roots.animation_to_world = orthonormalize(world);
    roots.world_to_animation = inverse_rigid(roots.animation_to_world);
}

}  // namespace s3::physics::skeleton_air_frames
