// Port of skate-core/src/physics/skeleton_root.rs.
#include "s3/core/physics/skeleton_root.hpp"

#include "s3/core/math.hpp"
#include "s3/core/physics/board_ground.hpp"
#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::physics::skeleton_root {

using board_ground::angle_between;
using board_motion_output::inverse_length_squared;
using board_motion_output::normalize_row_or;
using math::Vector3;
using native_arithmetic::dot3;
using skeleton_animation_record::compose_affine;
using skeleton_animation_record::IDENTITY;

void SkeletonRootFrames::update_teleport(AnimationPartTransform board_frame,
                                         const AnimationPartTransform& animation_board,
                                         const AnimationPartTransform& reckoning_frame_816) {
    initialize_heading = true;
    supplied_prediction = board_frame[3];
    update(board_frame, {0.0f, 0.0f, 0.0f, 0.0f}, 0.0f, animation_board, reckoning_frame_816);
    //82BDFF0C differs from ordinary update's previous-position snapshot.
    previous_board_position = board_frame[3];
}

void SkeletonRootFrames::reset_initial_alignment(AnimationPartTransform frame) {
    animation_to_world = frame;
    world_to_animation = inverse_rigid(frame);
}

void SkeletonRootFrames::update(AnimationPartTransform board_frame, std::array<float, 4> board_velocity,
                                float time_step, const AnimationPartTransform& animation_board,
                                const AnimationPartTransform& reckoning) {
    previous_board_position = board[3];
    board = board_frame;
    inverse_board = inverse_rigid(board_frame);
    if (supplied_prediction) {
        predicted_board_position = *supplied_prediction;
        supplied_prediction.reset();
    } else {
        for (std::size_t i = 0; i < 4; ++i) {
            predicted_board_position[i] = rs::mul_add(board_velocity[i], time_step, board_frame[3][i]);
        }
    }
    if (initialize_heading) {
        //8296EC98 uses one rsqrt refinement, angle in[0,2pi], not atan2.
        const Vector3 at{animation_board[2][0], 0.0f, animation_board[2][2]};
        const Vector3 forward{0.0f, 0.0f, 1.0f};
        float angle = angle_between(at, forward);
        // Normalize before the sign test, exactly as the angle helper.
        const std::array<float, 4> at4{at.x, at.y, at.z, 0.0f};
        const float sq = dot3(at4, at4);
        if (sq > rs::from_bits(0x38D1B717u)) {
            const float inverse = inverse_length_squared(sq, 1);
            const float x = at.x * inverse;
            // dot(cross(normalized_at, forward), world_up) == -x.
            if (0.0f > -x) {
                angle = rs::from_bits(0x40C90FDBu) - angle;
            }
        }
        const auto [sin, cos] = trigonometry::sin_cos(angle);
        heading_alignment = {{
            {cos, 0.0f, -sin, 0.0f},
            {0.0f, 1.0f, 0.0f, 0.0f},
            {sin, 0.0f, cos, 0.0f},
            {0.0f, 0.0f, 0.0f, 0.0f},
        }};
        initialize_heading = false;
    }
    AnimationPartTransform remove_board_translation = IDENTITY;
    for (std::size_t i = 0; i < 4; ++i) remove_board_translation[3][i] = -animation_board[3][i];
    const AnimationPartTransform local = compose_affine(heading_alignment, remove_board_translation);
    animation_to_board = compose_affine(reckoning, local);
    AnimationPartTransform world_translation = IDENTITY;
    world_translation[3] = predicted_board_position;
    animation_to_world = orthonormalize(compose_affine(world_translation, animation_to_board));
    world_to_animation = inverse_rigid(animation_to_world);
}

AnimationPartTransform orthonormalize(AnimationPartTransform source) {
    AnimationPartTransform result = source;
    for (std::size_t axis = 3; axis-- > 0;) {
        std::array<float, 4> vector = source[axis];
        for (std::size_t prior = 3; prior-- > axis + 1;) {
            const float projection = dot3(result[prior], source[axis]);
            for (std::size_t lane = 0; lane < 4; ++lane) {
                vector[lane] -= result[prior][lane] * projection;
            }
        }
        result[axis] = normalize_row_or(vector, IDENTITY[axis]);
    }
    return result;
}

AnimationPartTransform inverse_rigid(const AnimationPartTransform& source) {
    AnimationPartTransform result = IDENTITY;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        result[axis] = {source[0][axis], source[1][axis], source[2][axis], 0.0f};
    }
    std::array<float, 4> translation{};
    for (std::size_t i = 0; i < 4; ++i) translation[i] = 0.0f - source[3][i];
    for (std::size_t i = 0; i < 4; ++i) {
        const float z = translation[2] * result[2][i];
        const float y = rs::mul_add(translation[1], result[1][i], z);
        result[3][i] = rs::mul_add(translation[0], result[0][i], y);
    }
    return result;
}

}  // namespace s3::physics::skeleton_root
