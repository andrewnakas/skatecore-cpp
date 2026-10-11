// Port of skate-core/src/animation/foot_ik/transforms.rs.
#include "s3/core/animation/foot_ik/transforms.hpp"

namespace s3::animation::foot_ik::transforms {

using physics::skeleton_animation_record::compose_affine;
using status::Mode;

void prepare_animation_target(status::LimbStatus& status, LimbFrames& frames, LimbBinding binding,
                              const std::array<Transform, 24>& animation, const Transform& animation_to_world,
                              const Transform& inverse_animation_board, std::array<float, 4> contact_bounds) {
    if (status.mode != Mode::OnDeck && !(status.board_blend > 0.0f) && status.external_blend >= 1.0f) {
        return;
    }
    const Transform& animated = animation[binding.part];
    frames.board = compose_affine(inverse_animation_board, frames.target);
    status.part_position = frames.board[3];
    frames.world = compose_affine(animation_to_world, animated);
    // Native comparisons reject only a strict bound violation; NaNs do not
    // become a host-side exclusion. The fourth stored lane is excluded.
    bool within = true;
    for (std::size_t i = 0; i < 3; ++i) {
        if (status.part_position[i] > contact_bounds[i] || -contact_bounds[i] > status.part_position[i]) {
            within = false;
            break;
        }
    }
    frames.within_contact_bounds = within;
    if (binding.parent_part) {
        const Transform relative = compose_affine(inverse_rigid(animated), animation[*binding.parent_part]);
        frames.parent_board = compose_affine(frames.board, relative);
        frames.parent_world = compose_affine(frames.world, relative);
    }
}

}  // namespace s3::animation::foot_ik::transforms
