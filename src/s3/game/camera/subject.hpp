// Port of simref sim/src/camera/subject.rs.
//! Typed physics/animation publication boundary for normal subject82DF69C0.
//! The simulation supplies current physical outputs; camera history stays here.
#pragma once
#include <cstdint>
#include <optional>
#include <string>

#include "s3/core/camera/anchors.hpp"
#include "s3/core/camera/compass.hpp"
#include "s3/core/camera/manager.hpp"
#include "s3/core/camera/manager_subject.hpp"
#include "s3/core/camera/subject.hpp"
#include "s3/core/camera/subject_pose.hpp"
#include "s3/game/camera/graph_subject.hpp"

namespace s3::game::camera {

struct CameraSubjectSnapshot {
    /// Completed physics tick that produced every field in this snapshot.
    std::uint64_t tick = 0;
    /// Physical flags, trajectory, steering, root and other direct outputs.
    /// The camera overwrites this value's transform, reference_positions,
    /// anchors and compass with the source publishers below before use.
    s3::camera::ManagerSubject subject;
    s3::camera::SubjectPoseInputs pose;
    s3::camera::AnchorInputs anchors;
    /// Bone positions and raw COM; tracked_anchor, incline_normal and damped
    /// COM are filled by their camera owners immediately before publication.
    s3::camera::ReferencePointInputs reference_points;
    s3::camera::CompassPoseInputs compass;
    CameraGraphSubject graph;
};

namespace subject {

struct SubjectPublisher {
    // private in Rust
    s3::camera::SubjectPosePublisher pose;
    s3::camera::Anchors anchors;
    s3::camera::Compass compass;
    std::optional<std::uint64_t> last_tick;

    static SubjectPublisher new_();

    rs::Result<s3::camera::ManagerSubject, std::string> publish(CameraSubjectSnapshot input,
                                                                const s3::camera::CameraMan& manager,
                                                                s3::camera::CompassSettings settings);
};

}  // namespace subject
}  // namespace s3::game::camera
