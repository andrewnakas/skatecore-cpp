// Port of skate-core/src/camera/vector_tracker.rs.
//! TU3 vector-tracker candidates.
#pragma once
#include "s3/core/camera/_types.hpp"
#include "s3/core/camera/tracker.hpp"
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/reciprocal_sqrt.hpp"

namespace s3::camera {

/// Four lanes are retained because native vector stores update all four.
/// Magnitudes and direction tests use xyz only.
struct VectorTracker {
    F4 target{};
    F4 position{};
    F4 velocity{};
    F4 acceleration{};
    float acceleration_clamp = 0.0f;
    float smoothing = 0.0f;

    void update(float dt, F4 target, const ScalarTrackerParameters& p);
};
S3_DERIVE_EQ(VectorTracker, target, position, velocity, acceleration, acceleration_clamp, smoothing)

namespace vector_tracker {

inline float dot(const F4& a, const F4& b) { return physics::native_arithmetic::dot3(a, b); }

inline float refined_reciprocal(float value) {
    float estimate = physics::native_arithmetic::reciprocal_estimate(value);
    for (int i = 0; i < 2; ++i) {
        estimate = rs::mul_add(estimate, rs::mul_add(-estimate, value, 1.0f), estimate);
    }
    return estimate;
}

inline float length(const F4& value) {
    const float square = dot(value, value);
    float estimate = physics::reciprocal_sqrt::estimate(square);
    for (int i = 0; i < 2; ++i) {
        const float residual = rs::mul_add(-square, estimate * estimate, 1.0f);
        estimate = rs::mul_add(estimate * 0.5f, residual, estimate);
    }
    if (square == 0.0f) {
        return 0.0f;
    }
    return square * estimate;
}

}  // namespace vector_tracker

/// TU3 82E08D18 preserves the fourth lane unless the xyz magnitude is capped;
/// then every lane receives the same multiplier.
inline F4 clamp_magnitude(F4 value, float limit) {
    const float magnitude = vector_tracker::length(value);
    if (magnitude > limit) {
        const float scale = limit / magnitude;
        return rs::map(value, [&](float v) { return v * scale; });
    }
    return value;
}

}  // namespace s3::camera
