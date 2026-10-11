// Port of skate-core/src/camera/rig_tracking.rs.
//! Complete normal-rig tracking stages. Runtime parameters are explicit native
//! bindings; this does not initialize a shot or infer stock camera settings.
#pragma once
#include <cstdint>

#include "s3/core/camera/_types.hpp"
#include "s3/core/camera/angle_tracker.hpp"
#include "s3/core/camera/vector_tracker.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::camera {

using point_graph::PointGraph;

struct AngleTrackingSettings {
    float speed_clamp_degrees = 0.0f;
    float acceleration_min_degrees = 0.0f;
    float acceleration_max_degrees = 0.0f;
    float delta_umbra_degrees = 0.0f;
    float delta_penumbra_degrees = 0.0f;
};
S3_DERIVE_EQ(AngleTrackingSettings, speed_clamp_degrees, acceleration_min_degrees, acceleration_max_degrees,
             delta_umbra_degrees, delta_penumbra_degrees)

struct AngularRigTracking {
    AngleTracker heading;
    ScalarTrackerParameters heading_parameters;
    AngleTracker elevation;
    ScalarTrackerParameters elevation_parameters;
    float heading_target = 0.0f;
    float elevation_target = 0.0f;
    float heading_offset = 0.0f;
    float reset_time = 0.0f;
    float heading_smoothing = 0.0f;
    float elevation_smoothing = 0.0f;
    /// Native rig484 controls both the heading gate and acceleration doubling.
    float heading_acceleration_state = 0.0f;
    /// Native rig517; bit0x20 enables heading update independently of rig484.
    std::uint8_t flags_517 = 0;

    /// Complete82E030C0. This VMX wrap differs from scalar8258DB98.
    float current_heading() const;
    /// Complete82E054D8, including the preserve-velocity reset branch.
    void reset_heading(bool clear_motion);
    /// Complete82E05558.
    void reset_elevation(bool clear_motion);
    /// Complete82E03C18 with parameter-block fields516..532.
    void update_heading(float dt, AngleTrackingSettings settings);
    /// Complete82E03E28 with parameter-block fields560..576.
    void update_elevation(float dt, AngleTrackingSettings settings);
};
S3_DERIVE_EQ(AngularRigTracking, heading, heading_parameters, elevation, elevation_parameters, heading_target,
             elevation_target, heading_offset, reset_time, heading_smoothing, elevation_smoothing,
             heading_acceleration_state, flags_517)

namespace rig_tracking {
/// pub(super).
float wrap_vmx(float angle);
}  // namespace rig_tracking

/// Native subject getter slots used by82E04360/82E04478/82E041B8.
/// Slot numbers remain explicit until the coordinator binds the subject producers.
struct AnchorTrackingSubject {
    virtual ~AnchorTrackingSubject() = default;
    virtual std::uint8_t flag_584() = 0;
    virtual std::uint8_t flag_588() = 0;
    virtual float value_476() = 0;
    virtual std::uint8_t flag_548() = 0;
};

struct AnchorTrackingSettings {
    /// Native X320/Y352, eight points.
    PointGraph<8> latch_curve;
    /// Native X448/Y480, eight points.
    PointGraph<8> input_curve;
    float speed_clamp = 0.0f;
    float acceleration_clamp = 0.0f;
    /// Native fields600,624,808.
    float latch_scale = 0.0f;
    float input_threshold = 0.0f;
    float input_scale = 0.0f;
};
S3_DERIVE_EQ(AnchorTrackingSettings, latch_curve, input_curve, speed_clamp, acceleration_clamp, latch_scale,
             input_threshold, input_scale)

struct AnchorRigTracking {
    VectorTracker tracker;
    ScalarTrackerParameters parameters;
    F4 anchor{};
    F4 velocity{};
    float reference_height = 0.0f;
    float latch_time = 0.0f;
    float input_time = 0.0f;
    std::uint8_t flags_516 = 0;
    std::uint8_t flags_517 = 0;

    /// Complete82E04360, including the conditional second subject getter.
    float latch_smoothing(float dt, const AnchorTrackingSettings& s, AnchorTrackingSubject& subject);
    /// Complete82E04478.
    float input_smoothing(float dt, const AnchorTrackingSettings& s, AnchorTrackingSubject& subject);
    /// Complete82E041B8. Both smoothing histories update before the disable gate.
    void update(float dt, const AnchorTrackingSettings& s, AnchorTrackingSubject& subject);
};
S3_DERIVE_EQ(AnchorRigTracking, tracker, parameters, anchor, velocity, reference_height, latch_time, input_time,
             flags_516, flags_517)

/// Subject getter slots consumed by the complete reference-height stage82E03F20.
struct ReferenceHeightSubject {
    virtual ~ReferenceHeightSubject() = default;
    virtual std::uint8_t flag_656() = 0;
    virtual std::uint8_t flag_608() = 0;
};

struct ReferenceHeightTracking {
    float anchor_height = 0.0f;
    float transition_start = 0.0f;
    float transition_end = 0.0f;
    float transition_weight = 0.0f;
    float transition_duration = 0.0f;
    float transition_time = 0.0f;
    float height = 0.0f;
    float previous_height = 0.0f;
    float height_offset = 0.0f;
    std::uint32_t mode = 0;
    std::uint8_t flags_516 = 0;
    std::uint8_t flags_517 = 0;

    /// Complete82E03F20, including zero/initial height, transitions and the
    /// conditional subject-query order. No mode is invented or selected here.
    void update(float dt, ReferenceHeightSubject& subject);
};
S3_DERIVE_EQ(ReferenceHeightTracking, anchor_height, transition_start, transition_end, transition_weight,
             transition_duration, transition_time, height, previous_height, height_offset, mode, flags_516,
             flags_517)

}  // namespace s3::camera
