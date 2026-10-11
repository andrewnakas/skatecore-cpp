// Port of skate-core/src/physics/skeleton_body/target_update.rs.
#include "s3/core/physics/board_pose.hpp"
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/riding/ground_orientation.hpp"

namespace s3::physics::skeleton_body::targets {

using math::Vector3;
using native_arithmetic::dot3;
using riding::ground_orientation::clamp_length;
using skeleton_animation_record::compose_affine;
using target_update::ExtraTargetPositions;
using target_update::SkeletonTargetInput;
using target_update::SkeletonTargetUpdate;
using target_update::Transform;

namespace {
Transform orthonormalize(const Transform& frame) {
    std::array<std::uint32_t, 16> w{};
    for (std::size_t i = 0; i < 16; ++i) w[i] = rs::to_bits(frame[i / 4][i % 4]);
    const auto words = board_pose::orthonormalize_rotation(w);
    Transform out{};
    for (std::size_t i = 0; i < 4; ++i)
        for (std::size_t j = 0; j < 4; ++j) out[i][j] = rs::from_bits(words[i * 4 + j]);
    return out;
}
}  // namespace

Transform SkeletonTargets::update_hook_positions(const SkeletonTargetInput& input) {
    const Transform hips = compose_affine(input.animation_to_world, input.animation_hips);
    const Transform board = compose_affine(input.animation_to_world, input.animation_board);
    const Transform animation_board_to_physics = compose_affine(input.inverse_board, board);
    set_transform(0, orthonormalize(hips));
    set_transform(1, orthonormalize(input.skate_root));
    return animation_board_to_physics;
}

SkeletonTargetUpdate SkeletonTargets::update_positions(const SkeletonTargetInput& input, SkeletonBody& skeleton) {
    const auto previous = transform(0)[3];
    const Transform animation_board_to_physics = update_hook_positions(input);
    const auto hips = transform(0)[3];
    std::array<float, 4> change{};
    for (std::size_t i = 0; i < 4; ++i) change[i] = previous[i] - hips[i];
    const ExtraTargetPositions positions = update_extra_targets(skeleton, input.com_frame, input.lifted_com_frame);
    //82BE18A0 checks strictly greater; unordered distance alone does not
    //force this returnfalse. Preserve the independent teleport byte test.
    const bool continuous = !input.teleporting && !(dot3(change, change) > 1.0f);
    return SkeletonTargetUpdate{animation_board_to_physics, positions, continuous};
}

ExtraTargetPositions SkeletonTargets::update_extra_targets(SkeletonBody& skeleton, const Transform& com_frame,
                                                           const Transform& lifted_com_frame) {
    set_transform(2, lifted_com_frame);
    set_transform(3, com_frame);
    const ExtraTargetPositions positions{com_frame[3], lifted_com_frame[3], com_frame[3]};
    //GetPartTransform includes the actual local mass frame. Keep that
    //source even though these centered extra capsules have identity frames.
    const auto parts = skeleton.part_transforms();
    const std::pair<std::size_t, std::array<float, 4>> pairs[2] = {{24, lifted_com_frame[3]}, {25, com_frame[3]}};
    for (const auto& [part, target] : pairs) {
        const auto current = parts[part][3];
        const Vector3 velocity{
            (target[0] - current[0]) * 60.0f,
            (target[1] - current[1]) * 60.0f,
            (target[2] - current[2]) * 60.0f,
        };
        skeleton.bodies_mut()[part].rates.linear_velocity = clamp_length(velocity, 40.0f);
    }
    return positions;
}

}  // namespace s3::physics::skeleton_body::targets
