// Port of skate-core/src/camera/rig.rs (+ rig_initialization.rs: new/reset/teleport/set_elevation).
//! Native normal-camera rig update82E03300. The rig owns one copy of each
//! shared field; operation adapters below cannot retain inconsistent snapshots.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "s3/core/camera/_types.hpp"
#include "s3/core/camera/angle_tracker.hpp"
#include "s3/core/camera/avoidance.hpp"
#include "s3/core/camera/path_evaluator.hpp"
#include "s3/core/camera/positioner.hpp"
#include "s3/core/camera/rig_framing.hpp"
#include "s3/core/camera/rig_positioning.hpp"
#include "s3/core/camera/rig_tracking.hpp"
#include "s3/core/camera/vector_tracker.hpp"

namespace s3::camera {

struct Breadcrumbs {
    std::vector<F4> positions;
    std::size_t index = 0;
    float distance_squared_threshold = 0.0f;

    // private in Rust
    F4 last() const;
    void update(F4 position);
};
S3_DERIVE_EQ(Breadcrumbs, positions, index, distance_squared_threshold)

struct RigSettings {
    AngleTrackingSettings heading;
    AngleTrackingSettings elevation;
    AnchorTrackingSettings anchor;
    AvoidanceSettings avoidance;
    RigPositioningSettings positioning;
    /// Parameter620 and native global830BD350 respectively.
    float collision_hold_duration = 0.0f;
    F4 normalization_threshold{};
};
S3_DERIVE_EQ(RigSettings, heading, elevation, anchor, avoidance, positioning, collision_hold_duration,
             normalization_threshold)

struct RigFields {
    F4 anchor{};
    F4 velocity{};
    F4 acceleration{};
    F4 offset{};
    float heading_target = 0.0f;
    float heading_reference = 0.0f;
    float elevation_reference = 0.0f;
    float elevation_target = 0.0f;
    float elevation_offset = 0.0f;
    float additional_elevation = 0.0f;
    float heading_offset = 0.0f;
    float avoidance_heading = 0.0f;
    float avoidance_elevation = 0.0f;
    float smoothed_acceleration = 0.0f;
    float previous_acceleration = 0.0f;
    float collision_hold_time = 0.0f;
    float reset_time = 0.0f;
    float disable_avoidance_time = 0.0f;
    float height_transition_start = 0.0f;
    float height_transition_end = 0.0f;
    float height_transition_weight = 0.0f;
    float height_transition_duration = 0.0f;
    float height_transition_time = 0.0f;
    float reference_height = 0.0f;
    float previous_height = 0.0f;
    float height_offset = 0.0f;
    float latch_time = 0.0f;
    float input_time = 0.0f;
    float distance = 0.0f;
    float heading_smoothing = 0.0f;
    float elevation_smoothing = 0.0f;
    float heading_weight = 0.0f;
    float elevation_weight = 0.0f;
    float mode_time = 0.0f;
    std::uint32_t frame_counter = 0;
    std::uint32_t collision_mode = 0;
    std::uint32_t avoidance_mode = 0;
    std::uint32_t height_mode = 0;
    std::uint32_t previous_height_mode = 0;
    std::uint8_t flags_516 = 0;
    std::uint8_t flags_517 = 0;
};
/// Rust derived PartialEq (41 fields: more than S3_DERIVE_EQ takes).
inline bool operator==(const RigFields& a, const RigFields& b) {
    return a.anchor == b.anchor && a.velocity == b.velocity && a.acceleration == b.acceleration &&
           a.offset == b.offset && a.heading_target == b.heading_target &&
           a.heading_reference == b.heading_reference && a.elevation_reference == b.elevation_reference &&
           a.elevation_target == b.elevation_target && a.elevation_offset == b.elevation_offset &&
           a.additional_elevation == b.additional_elevation && a.heading_offset == b.heading_offset &&
           a.avoidance_heading == b.avoidance_heading && a.avoidance_elevation == b.avoidance_elevation &&
           a.smoothed_acceleration == b.smoothed_acceleration &&
           a.previous_acceleration == b.previous_acceleration && a.collision_hold_time == b.collision_hold_time &&
           a.reset_time == b.reset_time && a.disable_avoidance_time == b.disable_avoidance_time &&
           a.height_transition_start == b.height_transition_start &&
           a.height_transition_end == b.height_transition_end &&
           a.height_transition_weight == b.height_transition_weight &&
           a.height_transition_duration == b.height_transition_duration &&
           a.height_transition_time == b.height_transition_time && a.reference_height == b.reference_height &&
           a.previous_height == b.previous_height && a.height_offset == b.height_offset &&
           a.latch_time == b.latch_time && a.input_time == b.input_time && a.distance == b.distance &&
           a.heading_smoothing == b.heading_smoothing && a.elevation_smoothing == b.elevation_smoothing &&
           a.heading_weight == b.heading_weight && a.elevation_weight == b.elevation_weight &&
           a.mode_time == b.mode_time && a.frame_counter == b.frame_counter &&
           a.collision_mode == b.collision_mode && a.avoidance_mode == b.avoidance_mode &&
           a.height_mode == b.height_mode && a.previous_height_mode == b.previous_height_mode &&
           a.flags_516 == b.flags_516 && a.flags_517 == b.flags_517;
}
inline bool operator!=(const RigFields& a, const RigFields& b) { return !(a == b); }

/// Rust `S: RigModeSubject + AvoidanceSubject + ReferenceHeightSubject + AnchorTrackingSubject
/// + RigPositioningSubject` (the bound of `Rig::update`) as one combined interface.
struct RigSubject : RigModeSubject,
                    AvoidanceSubject,
                    ReferenceHeightSubject,
                    AnchorTrackingSubject,
                    RigPositioningSubject {};

struct Rig {
    RigFields fields;
    AngleTracker heading;
    ScalarTrackerParameters heading_parameters;
    AngleTracker elevation;
    ScalarTrackerParameters elevation_parameters;
    VectorTracker anchor;
    ScalarTrackerParameters anchor_parameters;
    Positioner positioner;
    std::array<AvoidancePath, 3> paths{};
    Breadcrumbs breadcrumbs;

    // ---- rig_initialization.rs ----
    static Rig new_(std::uint32_t avoidance_mode);
    /// Reset preserves native subobject history, avoidance mode and the four
    /// low flag bits in517. Orientation is reset separately by its owner.
    void reset();
    /// Teleport to this frame's native anchor and shot distance.
    void teleport();
    /// SetElevation82E03150: obstruction is latched until the native reset.
    void set_elevation(float elevation, bool use_avoidance);

    // ---- rig.rs ----
    void update(float dt, const RigSettings& settings, RigSubject& subject,
                std::array<TrajectoryCollisionRequest*, 3>& requests, MovingObstacleProvider& moving,
                PositionerCollisionProvider& collision);
    /// pub(super)
    AngularRigTracking angular() const;
    /// pub(super)
    void store_angular(const AngularRigTracking& a);

private:
    RigFraming framing() const;
    void reset_angles();
};

}  // namespace s3::camera
