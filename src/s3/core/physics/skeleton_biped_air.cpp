// Port of skate-core/src/physics/skeleton_biped_air.rs.
#include "s3/core/physics/skeleton_biped_air.hpp"

namespace s3::physics::skeleton_biped_air {

using skeleton_animation_record::compose_affine;
using skeleton_root::orthonormalize;

Frame prepare(skeleton_root::SkeletonRootFrames& roots, Frame frame, std::array<float, 4> trajectory_position,
              std::array<float, 4> local_com, const Frame& mapped_board, std::uint32_t flags_2476,
              std::uint32_t& flags_2468) {
    roots.initialize_heading = true;
    if ((flags_2476 & 4u) != 0) {
        for (std::size_t axis : {0, 2}) {
            for (auto& lane : frame[axis]) lane = -lane;
        }
    }
    std::array<float, 4> translation{};
    for (std::size_t lane = 0; lane < 4; ++lane) {
        const float x = frame[0][lane] * local_com[0];
        const float y = rs::mul_add(frame[1][lane], local_com[1], x);
        translation[lane] = trajectory_position[lane] - rs::mul_add(frame[2][lane], local_com[2], y);
    }
    frame[3] = translation;
    roots.reset_initial_alignment(orthonormalize(frame));
    flags_2468 |= 0x80000u;
    return compose_affine(roots.animation_to_world, mapped_board);
}

}  // namespace s3::physics::skeleton_biped_air
