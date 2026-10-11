// Port of skate-core/src/camera/tracker.rs.
//! Complete scalar tracker update at TU3 0x82E08640 (0x190 bytes).
//! Callers supply native state, parameters and timestep; no camera tuning or
//! initialization policy is inferred here. See docs/agents/camera/CAM-01.md.
#pragma once
#include "s3/core/rs.hpp"

namespace s3::camera {

/// Effective tracker fields +28 through +56, after the native caller binds
/// stock settings and applies its dynamic overrides. These are not defaults.
struct ScalarTrackerParameters {
    float delta_umbra = 0.0f;
    float delta_penumbra = 0.0f;
    float speed_clamp = 0.0f;
    float acceleration_clamp_min = 0.0f;
    float acceleration_clamp_max = 0.0f;
    float smoothing_min = 0.0f;
    float smoothing_max = 0.0f;
    bool overshoot_zeroes_velocity = false;
};
/// The six scalar words written/read at tracker offsets +0 through +20.
/// Acceleration clamp retains its prior value when acceleration opposes error.
struct ScalarTracker {
    float target = 0.0f;
    float position = 0.0f;
    float velocity = 0.0f;
    float acceleration = 0.0f;
    float acceleration_clamp = 0.0f;
    float smoothing = 0.0f;

    /// Advance the complete native update. TU3 does not clamp dt, sanitize
    /// parameters or special-case dt == 0 in this function.
    void update(float dt, float target, const ScalarTrackerParameters& parameters);
};

S3_DERIVE_EQ(ScalarTracker, target, position, velocity, acceleration, acceleration_clamp, smoothing)
S3_DERIVE_EQ(ScalarTrackerParameters, delta_umbra, delta_penumbra, speed_clamp, acceleration_clamp_min,
             acceleration_clamp_max, smoothing_min, smoothing_max, overshoot_zeroes_velocity)

namespace tracker {
/// pub(super): the equal-parameter branch avoids a multiply/add, including for NaNs in
/// the unused fraction. Native otherwise uses a VMX fused multiply-add.
inline float interpolate(float minimum, float maximum, float fraction) {
    if (minimum == maximum) {
        return maximum;
    }
    return rs::mul_add(maximum - minimum, fraction, minimum);
}
}  // namespace tracker

}  // namespace s3::camera
