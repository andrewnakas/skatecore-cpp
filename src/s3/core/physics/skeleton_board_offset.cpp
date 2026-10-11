// Port of skate-core/src/physics/skeleton_board_offset.rs.
#include "s3/core/physics/skeleton_board_offset.hpp"

#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::physics::skeleton_board_offset {

using board_motion_output::normalize_row_or;
using native_arithmetic::dot3;
using skeleton_animation_record::compose_affine;
using skeleton_animation_record::IDENTITY;

void SkateboardOffset::refresh_transform(AnimationPartTransform t) {
    transform = t;
    orientation_frames = 15.0f;
    height_frames = 15.0f;
    orientation_refreshed = true;
    height_refreshed = true;
}

void SkateboardOffset::refresh_height(float height, float frames) {
    transform[3][1] = height;
    height_frames = frames;
    height_refreshed = true;
}

void SkateboardOffset::update(AnimationPartTransform& board_pose, std::array<AnimationPartTransform, 4>& targets) {
    bool apply = false;
    if (orientation_frames > 0.0f) {
        apply = true;
        if (!orientation_refreshed) {
            const float ratio = (orientation_frames - 1.0f) / orientation_frames;
            const float weight = ratio * ratio;
            const float identity_weight = 1.0f - weight;
            for (std::size_t axis = 0; axis < 3; ++axis) {
                for (std::size_t lane = 0; lane < 4; ++lane) {
                    float& value = transform[axis][lane];
                    value = rs::mul_add(value, weight, IDENTITY[axis][lane] * identity_weight);
                }
            }
            // vrlimi mask4 retains Y. X/Z/W belong to orientation decay.
            for (std::size_t lane : {0, 2, 3}) {
                transform[3][lane] *= weight;
            }
            const AnimationPartTransform unnormalized = transform;
            // Native Gram-Schmidt traverses At,Up,Ri in that order.
            for (std::size_t axis = 3; axis-- > 0;) {
                std::array<float, 4> vector = unnormalized[axis];
                for (std::size_t prior = 3; prior-- > axis + 1;) {
                    const float projection = dot3(transform[prior], unnormalized[axis]);
                    for (std::size_t lane = 0; lane < 4; ++lane) {
                        vector[lane] -= transform[prior][lane] * projection;
                    }
                }
                transform[axis] = normalize_row_or(vector, IDENTITY[axis]);
            }
            orientation_frames -= 1.0f;
        }
    }
    if (height_frames > 0.0f) {
        apply = true;
        if (!height_refreshed) {
            const float ratio = (height_frames - 1.0f) / height_frames;
            transform[3][1] *= ratio * ratio;
            height_frames -= 1.0f;
        }
    }
    if (apply) {
        board_pose = compose_affine(transform, board_pose);
        for (auto& target : targets) {
            target = compose_affine(transform, target);
        }
    }
    orientation_refreshed = false;
    height_refreshed = false;
}

}  // namespace s3::physics::skeleton_board_offset
