// Port of skate-core/src/physics/skeleton_body/collision_feedback.rs.
#include "s3/core/physics/skeleton_body.hpp"

namespace s3::physics::skeleton_body::collision_feedback {

SkeletonCollisionFeedback SkeletonCollisionFeedback::new_(SkeletonFeedbackSettings settings) {
    SkeletonCollisionFeedback out;
    const std::size_t specific_parts[2] = {23, 1};
    const V local_points[2] = {settings.groin_offset, settings.face_offset};
    const float radii[2] = {settings.groin_radius, settings.face_radius};
    for (std::size_t i = 0; i < 2; ++i) {
        out.specific[i] = SpecificContact{
            specific_parts[i], local_points[i], V{0.0f, 0.0f, 0.0f, 0.0f}, radii[i] * radii[i], false, false,
        };
    }
    out.settings = settings;
    out.contact_age.fill(0.0f);
    out.priority.fill(0.0f);
    out.compliant = settings.body.compliant;
    out.current.fill(false);
    out.bones.fill(BoneContact{});
    out.regions.fill(ContactRegion{});
    out.planes.clear();
    out.planes.reserve(20);
    out.highest_normal = {0.0f, 1.0f, 0.0f, 0.0f};
    out.foot_normal = {0.0f, 1.0f, 0.0f, 0.0f};
    out.material_normals = {};
    out.timer = 0.0f;
    out.drive_weight = 0.0f;
    out.maximum_priority = 0.0f;
    out.wipeout_times = {};
    out.maximum_skater_force = 0.0f;
    out.other_skater = -1;
    out.maximum_group_8_force = 0.0f;
    out.maximum_group_11_force = 0.0f;
    out.material_12_height = 0.0f;
    out.flags = SkeletonContactFlags{};
    return out;
}

void SkeletonCollisionFeedback::reset() {
    const auto saved_specific = specific;
    const auto saved_compliant = compliant;
    const bool any = flags.any;
    *this = new_(settings);
    specific = saved_specific;
    compliant = saved_compliant;
    flags.any = any;
}

void SkeletonCollisionFeedback::set_up_normal() {
    reset();
    priority = settings.body.priority;
    compliant = settings.body.compliant;
}

}  // namespace s3::physics::skeleton_body::collision_feedback
