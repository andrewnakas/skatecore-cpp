// Port of skate-core/src/physics/skeleton_body/drive_frames.rs (included via skeleton_body.hpp).
// BoneDrives::SetDriveFrames82BD6FD0. A common animation-space frame is
// expressed in the child and parent coordinates before quaternion packing.
#pragma once
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"

namespace s3::physics::skeleton_body::drive_frames {

using physics::drive_frames::RetailDriveFrame;
using physics::drive_frames::RetailDriveFrames;
using skeleton_animation_record::AnimationPartTransform;

RetailDriveFrames bone_drive_frames(const AnimationPartTransform& child_animation,
                                    const AnimationPartTransform& inverse_child,
                                    const AnimationPartTransform& inverse_parent);

/// Step_Solver2 normalizes each active pair before building the drive rows.
RetailDriveFrames prepare_bone_drive_frames(RetailDriveFrames frames);

}  // namespace s3::physics::skeleton_body::drive_frames

namespace s3::physics::skeleton_body {
using drive_frames::bone_drive_frames;
using drive_frames::prepare_bone_drive_frames;
}  // namespace s3::physics::skeleton_body
