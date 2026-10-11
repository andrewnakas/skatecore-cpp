// Port of skate-core/src/animation/pose_mirror.rs.
#include "s3/core/animation/pose_mirror.hpp"

#include <utility>

#include "s3/core/animation/pose_trajectory.hpp"

namespace s3::animation::pose_mirror {

namespace {
void reflect(Sqt& pose) {
    pose.rotation[0] *= -1.0f;
    pose.rotation[1] *= -1.0f;
    pose.translation[2] *= -1.0f;
}
}  // namespace

rs::Result<rs::Unit, std::string> mirror(Slice<Sqt> pose, Slice<const std::int32_t> parents,
                                         Slice<const std::int32_t> partners, std::uint32_t trajectory_mode) {
    if (pose.size() != parents.size() || pose.size() != partners.size()) {
        return rs::make_err(std::string("Mirror pose and hierarchy dimensions differ"));
    }
    for (std::size_t i = 0; i < partners.size(); ++i) {
        const std::int32_t partner = partners[i];
        if (partner < -1 || partner >= static_cast<std::int32_t>(pose.size())) {
            return rs::make_err("Invalid mirror partner for bone" + std::to_string(i));
        }
        // -1 excludes a bone; the smaller index processes both sides of a pair.
        if (partner < static_cast<std::int32_t>(i)) continue;
        if (trajectory_mode == 1 && (i == 0 || parents[i] == 0)) {
            if (i != 0) {
                // Literal quaternion8232F740, before the alternate reflection.
                pose[i].rotation = pose_trajectory::multiply(pose[i].rotation, {0.0f, 1.0f, 0.0f, 0.0f});
            }
            pose[i].rotation[1] *= -1.0f;
            pose[i].rotation[2] *= -1.0f;
            pose[i].translation[0] *= -1.0f;
        } else {
            reflect(pose[i]);
        }
        if (partner > static_cast<std::int32_t>(i)) {
            std::swap(pose[i], pose[static_cast<std::size_t>(partner)]);
            reflect(pose[i]);
        }
    }
    return rs::Unit{};
}

}  // namespace s3::animation::pose_mirror
