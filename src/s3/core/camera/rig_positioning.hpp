// Port of skate-core/src/camera/rig_positioning.rs.
//! TU3 rig-to-positioner operation82E037F0. Authored settings and the subject
//! adapter are inputs; the world provider retains the positioner's six-line ABI.
#pragma once
#include <cstddef>
#include <cstdint>

#include "s3/core/camera/_types.hpp"
#include "s3/core/camera/positioner.hpp"
#include "s3/core/camera/rig_framing.hpp"
#include "s3/core/camera/rig_tracking.hpp"

namespace s3::camera {

struct RigPositioningSubject {
    virtual ~RigPositioningSubject() = default;
    virtual std::uint8_t flag_668() = 0;
    virtual std::uint8_t flag_548() = 0;
    virtual std::uint8_t flag_660() = 0;
    virtual float value_524() = 0;
    virtual F4x4 transform_384() = 0;
    virtual F4 position_360(std::uint32_t index) = 0;
    virtual std::uint32_t context_532() = 0;
};

struct DistanceTrackingSettings {
    float speed_clamp = 0.0f;
    float smoothing = 0.0f;
    float acceleration_clamp = 0.0f;
};
S3_DERIVE_EQ(DistanceTrackingSettings, speed_clamp, smoothing, acceleration_clamp)

struct RigPositioningSettings {
    /// Parameter fields536/540/544 and548/552/556, selected by subject668.
    DistanceTrackingSettings normal;
    DistanceTrackingSettings alternate;
    /// Parameter fields908 and912, selected by subject548.
    float minimum_distance = 0.0f;
    float alternate_minimum_distance = 0.0f;
};
S3_DERIVE_EQ(RigPositioningSettings, normal, alternate, minimum_distance, alternate_minimum_distance)

struct RigPositioning {
    float heading_reference = 0.0f;
    float elevation_reference = 0.0f;
    float avoidance_heading = 0.0f;
    float avoidance_elevation = 0.0f;
    float elevation_weight = 0.0f;
    std::uint32_t collision_mode = 0;
    std::uint8_t flags_516 = 0;

    /// Rust `breadcrumbs: &mut [[f32; 4]]` -> pointer + count.
    void update(float dt, RigPositioningSettings settings, AngularRigTracking& angular, const RigFraming& framing,
                F4 tracked_anchor, Positioner& positioner, F4* breadcrumbs, std::size_t breadcrumb_count,
                RigPositioningSubject& subject, PositionerCollisionProvider& collision);

    /// Complete82E03150, including the optional avoidance-angle blend.
    void set_elevation(float elevation, bool avoidance, AngularRigTracking& angular, std::uint8_t positioner_flags);
};
S3_DERIVE_EQ(RigPositioning, heading_reference, elevation_reference, avoidance_heading, avoidance_elevation,
             elevation_weight, collision_mode, flags_516)

}  // namespace s3::camera
