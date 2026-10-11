// Port of skate-core/src/physics/board_pose.rs.
#include "s3/core/physics/board_pose.hpp"

namespace s3::physics::board_pose {

using namespace arithmetic;

void set_board_transform(std::array<PartPose, 7>& parts, PartPose& hook, PoseMatrix requested) {
    const M old_deck = matrix(part_transform(parts[6]));
    const PoseMatrix normalized = orthonormalize_rotation(requested);
    const M delta = compose(matrix(normalized), inverse_rigid(old_deck));
    set_part_transform(parts[6], normalized);
    for (std::size_t i = 0; i < 6; ++i) {
        const M moved = compose(delta, matrix(part_transform(parts[i])));
        set_part_transform(parts[i], orthonormalize_rotation(words(moved)));
    }
    set_part_transform(hook, requested);
}

}  // namespace s3::physics::board_pose
