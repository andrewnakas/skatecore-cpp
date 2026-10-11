// Port of skate-core/src/camera/subject_pose.rs.
//! Pose publication82DF80D8 and82DF7558..82DF76B4. Physical outputs remain
//! authoritative; this owns only the camera's proven one-frame pose history.
#pragma once
#include "s3/core/camera/_types.hpp"
#include "s3/core/camera/vector_tracker.hpp"

namespace s3::camera {

struct SubjectPoseInputs {
    F4x4 physical_transform{};
    F4x4 skeleton_root{};
    F4 center_of_mass{};
    F4 reckoned_center_of_mass{};
    /// State output7 bytes59 and75 select the skeleton root.
    bool wiping_out = false;
    bool state_flag_75 = false;
};
S3_DERIVE_EQ(SubjectPoseInputs, physical_transform, skeleton_root, center_of_mass, reckoned_center_of_mass,
             wiping_out, state_flag_75)

struct PublishedSubjectPose {
    F4x4 transform{};
    F4 damped_center_of_mass{};
};
S3_DERIVE_EQ(PublishedSubjectPose, transform, damped_center_of_mass)

struct SubjectPosePublisher {
    F4x4 previous_physical_transform{};  // private in Rust

    static SubjectPosePublisher new_() {
        // Subject constructor82E07C08 initializes its three transform matrices
        // from the native unit-axis constants, with a zero translation vector.
        SubjectPosePublisher p;
        p.previous_physical_transform = {{
            {1.0f, 0.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 0.0f, 0.0f},
        }};
        return p;
    }

    PublishedSubjectPose publish(const SubjectPoseInputs& input) {
        const F4x4 transform = (input.wiping_out || input.state_flag_75) ? input.skeleton_root
                                                                         : this->previous_physical_transform;
        // 82DF80D8 executes before publisher setter28, preserving that delay.
        this->previous_physical_transform = input.physical_transform;
        F4 damped = input.reckoned_center_of_mass;
        const F4 delta = rs::from_fn<4>([&](std::size_t i) { return damped[i] - input.center_of_mass[i]; });
        const float distance = vector_tracker::length(delta);
        const float maximum = rs::from_bits(0x3e147ae1);
        if (distance > maximum) {
            const float inverse = vector_tracker::refined_reciprocal(distance);
            damped = rs::from_fn<4>(
                [&](std::size_t i) { return rs::mul_add(inverse * delta[i], maximum, input.center_of_mass[i]); });
        }
        return PublishedSubjectPose{transform, damped};
    }
};
S3_DERIVE_EQ(SubjectPosePublisher, previous_physical_transform)

}  // namespace s3::camera
