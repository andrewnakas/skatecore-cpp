// Port of skate-core/src/physics/skeleton_motion.rs.
#include "s3/core/physics/skeleton_motion.hpp"

#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::physics::skeleton_motion {

using native_arithmetic::reciprocal_estimate;
using skeleton_animation_record::compose_affine;

void SkeletonMotion::reset_board_orientation_history() { previous_board_at_y = 0.0f; }

void SkeletonMotion::process_trajectory(const AnimationPartTransform& trajectory_bone,
                                        const AnimationPartTransform& animation_to_world, float dt) {
    trajectory = compose_affine(trajectory_bone, next_trajectory);
    next_trajectory = trajectory;
    float reciprocal = reciprocal_estimate(dt);
    for (int i = 0; i < 2; ++i) {
        const float error = rs::mul_add(-reciprocal, dt, 1.0f);
        reciprocal = rs::mul_add(reciprocal, error, reciprocal);
    }
    std::array<float, 4> velocity{};
    for (std::size_t i = 0; i < 4; ++i) velocity[i] = trajectory[3][i] * reciprocal;
    for (std::size_t lane = 0; lane < 4; ++lane) {
        const float x = animation_to_world[0][lane] * velocity[0];
        const float y = rs::mul_add(animation_to_world[1][lane], velocity[1], x);
        velocity_world[lane] = rs::mul_add(animation_to_world[2][lane], velocity[2], y);
    }
    const AnimationPartTransform source = trajectory;
    for (std::size_t axis = 0; axis < 4; ++axis) {
        if (axis < 3) {
            inverse_trajectory[axis] = {source[0][axis], source[1][axis], source[2][axis], 0.0f};
        } else {
            inverse_trajectory[axis] = {0.0f, 0.0f, 0.0f, 0.0f};
        }
    }
    std::array<float, 4> last{};
    for (std::size_t lane = 0; lane < 4; ++lane) {
        const float z = (0.0f - source[3][2]) * inverse_trajectory[2][lane];
        const float y = rs::mul_add(0.0f - source[3][1], inverse_trajectory[1][lane], z);
        last[lane] = rs::mul_add(0.0f - source[3][0], inverse_trajectory[0][lane], y);
    }
    inverse_trajectory[3] = last;
}

void SkeletonMotion::publish_unadjusted_board(const AnimationPartTransform& board, std::uint32_t& flags_2472) {
    if (rs::abs(board[0][1]) > rs::from_bits(0x3f35c28fu)) {
        flags_2472 |= 0x8000u;
    }
}

float SkeletonMotion::publish_adjusted_board(const AnimationPartTransform& board, std::uint32_t& flags_2468) {
    const float at_y = board[2][1];
    const float delta = at_y - previous_board_at_y;
    previous_board_at_y = at_y;
    const bool positive = at_y > 0.0f;
    flags_2468 = (flags_2468 & ~0x1800u) | (std::uint32_t(positive) << 12) | (std::uint32_t(!positive) << 11);
    return delta;
}

}  // namespace s3::physics::skeleton_motion
