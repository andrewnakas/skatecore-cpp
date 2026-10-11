// Port of skate-core/src/physics/skeleton_body/drive_frames.rs.
#include "s3/core/physics/drive_preparation.hpp"
#include "s3/core/physics/skeleton_body.hpp"

namespace s3::physics::skeleton_body::drive_frames {

using math::Basis3;
using math::Vector3;
using physics::drive_frames::retail_quaternion_from_basis;
using rigid_body::RetailQuaternion;
using skeleton_animation_record::compose_affine;
using skeleton_animation_record::IDENTITY;

namespace {
RetailDriveFrame pack(const AnimationPartTransform& frame) {
    Basis3 basis;
    for (std::size_t i = 0; i < 3; ++i) basis.columns[i] = {frame[i][0], frame[i][1], frame[i][2]};
    return RetailDriveFrame{retail_quaternion_from_basis(basis), Vector3{frame[3][0], frame[3][1], frame[3][2]}};
}
}  // namespace

RetailDriveFrames bone_drive_frames(const AnimationPartTransform& child_animation,
                                    const AnimationPartTransform& inverse_child,
                                    const AnimationPartTransform& inverse_parent) {
    AnimationPartTransform common = IDENTITY;
    common[3] = child_animation[3];
    const AnimationPartTransform child = compose_affine(inverse_child, common);
    const AnimationPartTransform parent = compose_affine(inverse_parent, common);
    return RetailDriveFrames{pack(child), pack(parent)};
}

RetailDriveFrames prepare_bone_drive_frames(RetailDriveFrames frames) {
    std::array<std::uint32_t, 16> words{};
    const RetailDriveFrame pair[2] = {frames.body_a, frames.body_b};
    for (std::size_t i = 0; i < 2; ++i) {
        const RetailQuaternion q = pair[i].orientation;
        const Vector3 p = pair[i].translation;
        const std::array<float, 8> values{q.x, q.y, q.z, q.w, p.x, p.y, p.z, 0.0f};
        for (std::size_t j = 0; j < 8; ++j) words[i * 8 + j] = rs::to_bits(values[j]);
    }
    drive_preparation::normalize_drive_frames(words);
    auto frame = [&](std::size_t offset) {
        auto f = [&](std::size_t i) { return rs::from_bits(words[offset + i]); };
        return RetailDriveFrame{RetailQuaternion{f(0), f(1), f(2), f(3)}, Vector3{f(4), f(5), f(6)}};
    };
    return RetailDriveFrames{frame(0), frame(8)};
}

}  // namespace s3::physics::skeleton_body::drive_frames
