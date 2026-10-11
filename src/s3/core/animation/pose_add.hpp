// Port of skate-core/src/animation/pose_add.rs.
// ACS immediate AddSQT828CCC08, used by SkaterAnim::AddBindPose82B98118.
#pragma once
#include "s3/core/animation/output.hpp"
#include "s3/core/animation/pose_trajectory.hpp"

namespace s3::animation::pose_add {

using output::Sqt;

/// Native operand B supplies the reference frame. The designated motion
/// operand supplies the channel weight, independently of operand ordering.
inline Sqt add(Sqt a, Sqt b, bool motion_is_a) {
    const std::array<float, 3> rotated =
        pose_trajectory::rotate(b.rotation, {a.translation[0], a.translation[1], a.translation[2]});
    Sqt out;
    for (std::size_t i = 0; i < 4; ++i) out.scale[i] = b.scale[i] * a.scale[i];
    out.rotation = pose_trajectory::multiply(b.rotation, a.rotation);
    out.translation = {
        rotated[0] + b.translation[0],
        rotated[1] + b.translation[1],
        rotated[2] + b.translation[2],
        motion_is_a ? a.translation[3] : b.translation[3],
    };
    return out;
}

}  // namespace s3::animation::pose_add
