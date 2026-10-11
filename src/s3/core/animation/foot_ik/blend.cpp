// Port of skate-core/src/animation/foot_ik/blend.rs.
#include "s3/core/animation/foot_ik/blend.hpp"

#include "s3/core/animation/foot_ik/math.hpp"
#include "s3/core/physics/skeleton_root.hpp"

namespace s3::animation::foot_ik::blend {

using physics::skeleton_animation_record::compose_affine;
using physics::skeleton_animation_record::transform_point;
using physics::skeleton_root::orthonormalize;
using status::Mode;

namespace {
Transform board_blend(const Transform& original, const Transform& target, float weight) {
    std::array<float, 4> translation{};
    for (std::size_t lane = 0; lane < 4; ++lane) {
        translation[lane] = rs::mul_add(target[3][lane], weight, original[3][lane] * (1.0f - weight));
    }
    // Each source frame is orthonormalized independently in Z/Y/X order before
    // the matrix interpolation call; translation is replaced afterwards.
    Transform a = orthonormalize(original);
    Transform b = orthonormalize(target);
    a[3] = {0.0f, 0.0f, 0.0f, 0.0f};
    b[3] = {0.0f, 0.0f, 0.0f, 0.0f};
    Transform output = math::interpolate(a, b, weight).first;
    output[3] = translation;
    return output;
}
}  // namespace

void update(const std::array<status::LimbStatus, 4>& statuses, std::array<transforms::LimbFrames, 4>& frames,
            const std::array<transforms::LimbBinding, 4>& bindings, const Transform& physical_board,
            const Transform& animation_to_world, const Transform& animated_board) {
    // IK160 supplies only the physical board rotation. Translation comes from
    // the actual animation record's board transformed through IK32.
    Transform board = physical_board;
    board[3] = transform_point(animation_to_world, animated_board[3]);
    for (std::size_t i = 0; i < 4; ++i) {
        const status::LimbStatus& status = statuses[i];
        transforms::LimbFrames& frame = frames[i];
        const transforms::LimbBinding& binding = bindings[i];
        if (status.mode == Mode::Disabled) continue;
        if (status.board_blend > 0.0f) {
            const Transform target = compose_affine(board, frame.board);
            frame.world = board_blend(frame.world, target, status.board_blend);
            if (binding.parent_part) {
                const Transform parent_target = compose_affine(board, frame.parent_board);
                frame.parent_world = board_blend(frame.parent_world, parent_target, status.board_blend);
            }
        }
        if (status.mode == Mode::External || status.mode == Mode::Local) {
            if (status.external_blend >= 1.0f) {
                frame.world = frame.external_world;
                frame.parent_world = frame.external_parent_world;
            } else {
                // The native external stage also updates the retained parent
                // frame for a limb without a parent, including at zero weight.
                frame.world = math::interpolate_affine(frame.world, frame.external_world, status.external_blend);
                frame.parent_world =
                    math::interpolate_affine(frame.parent_world, frame.external_parent_world, status.external_blend);
            }
        }
    }
}

}  // namespace s3::animation::foot_ik::blend
