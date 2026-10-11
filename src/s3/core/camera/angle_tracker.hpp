// Port of skate-core/src/camera/angle_tracker.rs.
//! TU3 angular-tracking candidates; exceptional wrap conversion is unavailable.
#pragma once
#include "s3/core/camera/tracker.hpp"

namespace s3::camera {

/// Angular state has the same six-word layout as the scalar tracker. Angles
/// and angular rates use radians; effective settings are explicitly supplied.
/// Rust tuple struct `AngleTracker(pub ScalarTracker)`.
struct AngleTracker {
    ScalarTracker _0;

    void update(float dt, float target, const ScalarTrackerParameters& parameters);
};
S3_DERIVE_EQ(AngleTracker, _0)

/// Candidate 8258DB98 wrapping sequence. Its fast interval is [-pi, pi), and
/// its reduction uses truncation followed by one correction, not rem_euclid.
/// Panics (rs::Panic) for the unavailable exceptional integer conversion.
float normalize_angle(float angle);

}  // namespace s3::camera
