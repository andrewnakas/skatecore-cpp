// Port of skate-core/src/camera/avoidance.rs.
//! Complete TU3 normal-camera avoidance82E04C20. Three native PathEvaluators
//! retain separate asynchronous requests. World/subject adapters are external.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/camera/_types.hpp"
#include "s3/core/camera/path_evaluator.hpp"
#include "s3/core/camera/positioner.hpp"

namespace s3::camera {

struct AvoidanceSubject {
    virtual ~AvoidanceSubject() = default;
    virtual F4x4 transform_376() = 0;
    virtual F4 vector_452() = 0;
    virtual F4 vector_444() = 0;
    virtual std::uint32_t context_532() = 0;
    virtual std::uint8_t flag_608() = 0;
    virtual std::uint8_t flag_548() = 0;
};

struct AvoidancePath {
    PathEvaluator evaluator;
    /// Native path byte56, cleared by each avoidance submission.
    std::uint8_t prediction_flag = 0;

    void set(PredictionPath path, std::uint8_t acceleration_flag);  // private in Rust
};
S3_DERIVE_EQ(AvoidancePath, evaluator, prediction_flag)

struct AvoidanceSettings {
    /// Parameter offsets628,632,636,640,920.
    float horizon = 0.0f;
    float heading_smoothing = 0.0f;
    float elevation_smoothing = 0.0f;
    float ease_out_time = 0.0f;
    float radius_padding = 0.0f;
};
S3_DERIVE_EQ(AvoidanceSettings, horizon, heading_smoothing, elevation_smoothing, ease_out_time, radius_padding)

struct RigAvoidance {
    F4 anchor{};
    F4 velocity{};
    F4 tracked_anchor{};
    float heading_target = 0.0f;
    float heading_reference = 0.0f;
    float elevation_target = 0.0f;
    float avoidance_heading = 0.0f;
    float avoidance_elevation = 0.0f;
    float reset_time = 0.0f;
    float disable_time = 0.0f;
    float height_transition_start = 0.0f;
    float height_transition_end = 0.0f;
    float height_transition_duration = 0.0f;
    float height_transition_time = 0.0f;
    float reference_height = 0.0f;
    float prediction_distance = 0.0f;
    float heading_weight = 0.0f;
    float elevation_weight = 0.0f;
    std::uint32_t avoidance_mode = 0;
    std::uint32_t height_mode = 0;
    std::uint8_t flags_516 = 0;
    std::array<AvoidancePath, 3> paths{};

    /// Complete82E04C20. The last breadcrumb is supplied from the native ring
    /// selector (capacity+index-1)%capacity; no sampled/fitted trail is generated.
    void update(float dt, AvoidanceSettings settings, const Positioner& positioner, F4 last_breadcrumb,
                AvoidanceSubject& subject, std::array<TrajectoryCollisionRequest*, 3>& requests,
                MovingObstacleProvider& moving);

private:
    /// Complete82E03060; subject transform+32 is its forward vector.
    float subject_speed(AvoidanceSubject& subject) const;
};

}  // namespace s3::camera
