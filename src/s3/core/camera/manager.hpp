// Port of skate-core/src/camera/manager.rs (+ manager_output.rs: CameraMan::finish_frame).
//! Normal CameraMan82DFEAA8/82DFEC90/82DFEE80. The host publishes the real
//! physics/animation subject, evaluates the stock graph between prepare and
//! update, and supplies the three native kinds of collision query.
#pragma once
#include <array>
#include <string>

#include "s3/core/camera/_types.hpp"
#include "s3/core/camera/drop_predictor.hpp"
#include "s3/core/camera/frame_composer.hpp"
#include "s3/core/camera/look_input.hpp"
#include "s3/core/camera/manager_frame.hpp"
#include "s3/core/camera/manager_state.hpp"
#include "s3/core/camera/manager_subject.hpp"
#include "s3/core/camera/path_evaluator.hpp"
#include "s3/core/camera/positioner.hpp"
#include "s3/core/camera/rig.hpp"
#include "s3/core/camera/rig_orientation.hpp"
#include "s3/core/camera/shake.hpp"
#include "s3/core/camera/shot_manager.hpp"

namespace s3::camera {

struct ManagerSettings {
    RigSettings rig;
    OrientationSettings orientation;
    FrameSettings framing;
    DropSettings drop;
    LookSettings look;
    ShakeSettings shake;
    float steering_threshold = 0.0f;
};
S3_DERIVE_EQ(ManagerSettings, rig, orientation, framing, drop, look, shake, steering_threshold)

/// Rust `impl PositionerCollisionProvider + DropCollisionProvider` (CameraMan::update's
/// collision argument) as one combined interface.
struct CollisionProvider : PositionerCollisionProvider, DropCollisionProvider {};

struct CameraMan {
    ManagerState state;
    Rig rig;
    ShotManager shots;
    RigOrientation orientation;
    FrameComposer composer;
    DropPredictor drop;
    LookInput look;
    std::array<ShakeEffect, 2> shake{};
    CameraFrame frame;
    // Rig transform0..32 is the previous subject orientation; the camera's
    // tracked orientation lives separately in RigOrientation (native64..96).
    F4 subject_forward{};  // private in Rust
    float heading_multiplier = 0.0f;  // private in Rust

    static CameraMan new_();

    /// Once per published subject, before the camera graph reads its conditions.
    /// Landing response consumes the previous velocity/normal before incline
    /// publication replaces them; swapping these operations changes impacts.
    void prepare(const ManagerSubject& subject, const ManagerSettings& settings);

    rs::Result<bool, std::string> set_shot(const std::string& name, bool force, const ManagerSubject& subject,
                                           const ShotDatabase& database);

    rs::Result<CameraFrame, std::string> update(float dt, ManagerSubject& subject, const ManagerSettings& settings,
                                                std::array<const ShakeSamples*, 2> samples,
                                                std::array<TrajectoryCollisionRequest*, 3>& requests,
                                                MovingObstacleProvider& moving, CollisionProvider& collision);

private:
    void bind_anchor(const ManagerSubject& subject, bool reset);
    /// manager_output.rs (pub(super)).
    void finish_frame(float dt, const ManagerSubject& subject, const ShakeSettings& settings,
                      std::array<const ShakeSamples*, 2> samples, bool reset);
};

}  // namespace s3::camera
