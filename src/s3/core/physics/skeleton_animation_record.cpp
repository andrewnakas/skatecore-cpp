// Port of skate-core/src/physics/skeleton_animation_record.rs.
#include "s3/core/physics/skeleton_animation_record.hpp"

namespace s3::physics::skeleton_animation_record {

using rigid_body::basis_from_quaternion;
using rigid_body::RetailQuaternion;

AnimationPartTransform physics_bone_frame(std::array<float, 4> quaternion,
                                          std::array<float, 4> translation) {
    const auto columns =
        basis_from_quaternion(RetailQuaternion{quaternion[0], quaternion[1], quaternion[2],
                                               quaternion[3]})
            .columns;
    const auto& ri = columns[0];
    const auto& up = columns[1];
    const auto& at = columns[2];
    return {{
        {ri[0], ri[1], ri[2], at[2]},
        {up[0], up[1], up[2], up[0]},
        {at[0], at[1], at[2], ri[1]},
        translation,
    }};
}

rs::Result<std::array<AnimationPartTransform, 24>, const char*> map_animation_parts(
    const AnimationPartTransform* global_bones, std::size_t global_bones_len,
    const std::array<std::size_t, 24>& bone_indices,
    const std::array<AnimationPartTransform, 24>& physics_frames) {
    for (std::size_t i : bone_indices) {
        if (i >= global_bones_len) {
            return rs::make_err("physics skeleton bone index is outside the animation hierarchy");
        }
    }
    return rs::from_fn<24>([&](std::size_t part) {
        const AnimationPartTransform& animation = global_bones[bone_indices[part]];
        const AnimationPartTransform& physics = physics_frames[part];
        return rs::from_fn<4>([&](std::size_t column) {
            if (column == 3) {
                return transform_point(animation, physics[3]);
            }
            return rs::from_fn<4>([&](std::size_t lane) {
                const float x = physics[column][0] * animation[0][lane];
                const float y = rs::mul_add(physics[column][1], animation[1][lane], x);
                return rs::mul_add(physics[column][2], animation[2][lane], y);
            });
        });
    });
}

SkeletonAnimationMasses SkeletonAnimationMasses::from_bone_data(
    std::array<Vector3, 24> sizes, std::array<std::uint32_t, 24> collision_shapes,
    bool head_has_hat) {
    const auto weights = rs::from_fn<24>([&](std::size_t i) {
        if (i == 0 || (collision_shapes[i] >= 3 && !(i == 1 && head_has_hat))) {
            return 0.0f;
        }
        return (sizes[i].x * sizes[i].y) * sizes[i].z;
    });
    return normalize(weights);
}

SkeletonAnimationMasses SkeletonAnimationMasses::normalize(std::array<float, 24> part_weights) {
    float total = 0.0f;
    for (std::size_t i = 0; i < part_weights.size(); ++i) {
        const float weight = part_weights[i];
        // Preserve native operand order, including signed zero/NaN cases.
        total = (i % 8 == 0 || i % 8 == 7) ? weight + total : total + weight;
    }
    const float reciprocal = 1.0f / total;
    return SkeletonAnimationMasses{
        part_weights,
        total,
        rs::map(part_weights, [&](float v) { return v * reciprocal; }),
    };
}

void SkeletonAnimationRecord::reset_history() {
    centre_of_mass = {0.0f, 0.0f, 0.0f, 0.0f};
    centre_of_mass_delta = {0.0f, 0.0f, 0.0f, 0.0f};
    com_to_deck_world = {0.0f, 0.0f, 0.0f, 0.0f};
    com_to_deck_world_delta = {0.0f, 0.0f, 0.0f, 0.0f};
}

void SkeletonAnimationRecord::update(const std::array<AnimationPartTransform, 24>& pose_,
                                     const AnimationPartTransform& animation_to_board,
                                     const SkeletonAnimationMasses& masses) {
    pose = pose_;
    std::array<float, 4> com{0.0f, 0.0f, 0.0f, 0.0f};
    for (std::size_t p = 0; p < 24; ++p) {
        const float weight = masses.fractional[p];
        for (std::size_t lane = 0; lane < 4; ++lane) {
            com[lane] = rs::mul_add(pose[p][3][lane], weight, com[lane]);
        }
    }
    centre_of_mass_delta = rs::from_fn<4>([&](std::size_t i) { return com[i] - centre_of_mass[i]; });
    centre_of_mass = com;
    // Native transforms both points separately then subtracts, retaining
    // each affine FMA's rounding; do not rotate their difference instead.
    const auto transformed_com = transform_point(animation_to_board, com);
    const auto transformed_deck = transform_point(animation_to_board, pose[0][3]);
    const auto relative =
        rs::from_fn<4>([&](std::size_t i) { return transformed_com[i] - transformed_deck[i]; });
    com_to_deck_world_delta = rs::from_fn<4>(
        [&](std::size_t i) { return (relative[i] - com_to_deck_world[i]) * reset_scalar; });
    com_to_deck_world = relative;
    reset_scalar = 1.0f;
}

Vector3 SkeletonAnimationRecord::com_to_deck() const {
    const auto& v = com_to_deck_world;
    return Vector3{v[0], v[1], v[2]};
}

std::array<float, 4> transform_point(const AnimationPartTransform& transform,
                                     std::array<float, 4> point) {
    return rs::from_fn<4>([&](std::size_t i) {
        const float x = rs::mul_add(transform[0][i], point[0], transform[3][i]);
        const float y = rs::mul_add(transform[1][i], point[1], x);
        return rs::mul_add(transform[2][i], point[2], y);
    });
}

AnimationPartTransform compose_affine(const AnimationPartTransform& parent,
                                      const AnimationPartTransform& child) {
    return rs::from_fn<4>([&](std::size_t column) {
        if (column == 3) {
            return transform_point(parent, child[3]);
        }
        return rs::from_fn<4>([&](std::size_t lane) {
            const float x = child[column][0] * parent[0][lane];
            const float y = rs::mul_add(child[column][1], parent[1][lane], x);
            return rs::mul_add(child[column][2], parent[2][lane], y);
        });
    });
}

}  // namespace s3::physics::skeleton_animation_record
