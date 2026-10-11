// Port of skate-core/src/camera/rig_orientation.rs.
//! Normal gameplay rig pitch/yaw tracking and roll, TU3 82E045E0.
#pragma once
#include <cstdint>

#include "s3/core/camera/_types.hpp"
#include "s3/core/camera/angle_tracker.hpp"
#include "s3/core/math.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::camera {

struct OrientationTrackerSettings {
    float acceleration_min_degrees = 0.0f;
    float acceleration_max_degrees = 0.0f;
    float smoothing_min = 0.0f;
    float delta_umbra_degrees = 0.0f;
    float delta_penumbra_degrees = 0.0f;
};
S3_DERIVE_EQ(OrientationTrackerSettings, acceleration_min_degrees, acceleration_max_degrees, smoothing_min,
             delta_umbra_degrees, delta_penumbra_degrees)

struct OrientationSettings {
    float pan_umbra_degrees = 0.0f;
    float pan_penumbra_degrees = 0.0f;
    point_graph::PointGraph<8> pan_smoothing_curve;
    float pan_minimum_smoothing = 0.0f;
    OrientationTrackerSettings pan;
    OrientationTrackerSettings tilt;
};
S3_DERIVE_EQ(OrientationSettings, pan_umbra_degrees, pan_penumbra_degrees, pan_smoothing_curve,
             pan_minimum_smoothing, pan, tilt)

struct RigOrientation {
    F4 target{};
    math::Basis3 basis;
    AngleTracker pitch;
    AngleTracker yaw;
    ScalarTrackerParameters pitch_parameters;
    ScalarTrackerParameters yaw_parameters;
    float pan_smoothing = 0.0f;
    float tilt_smoothing = 0.0f;
    float roll = 0.0f;
    float framing_mirror = 0.0f;

    /// Construction82E02738 and reset82E02E58. The angle parameters are host
    /// storage: every consumed numerical field is rebound before tracking.
    static RigOrientation new_();

    void update(float dt, const OrientationSettings& settings, float reset_time, std::uint8_t& flags_516,
                std::uint8_t flags_517);
};
S3_DERIVE_EQ(RigOrientation, target, basis, pitch, yaw, pitch_parameters, yaw_parameters, pan_smoothing,
             tilt_smoothing, roll, framing_mirror)

}  // namespace s3::camera
