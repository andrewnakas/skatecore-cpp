// Port of skate-core/src/camera/rig_positioning.rs.
#include "s3/core/camera/rig_positioning.hpp"

namespace s3::camera {
namespace {

/// Complete82DF3410: wrap delta, fused interpolation, then wrap result.
float blend_angle(float current, float target, float weight) {
    return rig_tracking::wrap_vmx(rs::mul_add(weight, rig_tracking::wrap_vmx(target - current), current));
}

float clamp_elevation(float value) {
    const float maximum = rs::from_bits(0x3fb2b8c2);
    value = -maximum - value >= 0.0f ? -maximum : value;
    return maximum - value >= 0.0f ? value : maximum;
}

}  // namespace

void RigPositioning::update(float dt, RigPositioningSettings settings, AngularRigTracking& angular,
                            const RigFraming& framing, F4 tracked_anchor, Positioner& positioner, F4* breadcrumbs,
                            std::size_t breadcrumb_count, RigPositioningSubject& subject,
                            PositionerCollisionProvider& collision) {
    const bool alternate = subject.flag_668() != 0;
    F4 anchor = tracked_anchor;
    anchor[1] = framing.reference_height;
    const float reference_heading = angular.current_heading();
    const float reference_elevation = clamp_elevation(framing.elevation_offset + angular.elevation._0.position);
    const float heading = angular.current_heading();
    const float elevation = clamp_elevation((framing.additional_elevation + framing.elevation_offset) +
                                            angular.elevation._0.position);
    const float distance = rs::is_nan(framing.distance) ? 1.0f : framing.distance;
    const float minimum_distance =
        subject.flag_548() != 0 ? settings.alternate_minimum_distance : settings.minimum_distance;
    const std::uint8_t clamp_height = subject.flag_660();
    const float floor_height = subject.value_524();
    const DistanceTrackingSettings tracking = alternate ? settings.alternate : settings.normal;
    const F4 up = subject.transform_384()[1];
    const F4 first = subject.position_360(3);
    const F4 second = subject.position_360(0);
    const F4 third_raw = subject.position_360(7);
    const F4 third =
        rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(up[i], rs::from_bits(0x3e19999a), third_raw[i]); });
    PositionerConfig config;
    config.anchor = anchor;
    config.offset = framing.offset;
    config.reference_heading = reference_heading;
    config.reference_elevation = reference_elevation;
    config.heading = heading;
    config.elevation = elevation;
    config.distance = distance;
    config.minimum_distance = minimum_distance;
    config.collision_enabled = this->collision_mode != 2 ? 1 : 0;
    config.bypass_distance_tracker = (angular.flags_517 & 0x40) != 0 ? 1 : 0;
    config.clamp_height = clamp_height;
    config.floor_height = floor_height;
    config.speed_clamp = tracking.speed_clamp;
    config.acceleration_clamp = tracking.acceleration_clamp;
    config.smoothing = tracking.smoothing;
    config.query_starts = {first, second, third};
    // Native BLE skips reset; unordered reset_time proceeds into the reset.
    if (!(angular.reset_time <= 0.0f)) {
        positioner.distance_tracker.target = distance;
        positioner.distance_tracker.position = distance;
        positioner.distance_tracker.velocity = 0.0f;
        positioner.distance_tracker.acceleration = 0.0f;
    }
    angular.flags_517 &= std::uint8_t(~0x40u);
    const std::uint32_t context = subject.context_532();
    positioner.update(dt, context, config, collision);
    if ((positioner.flags & 4) == 0) {
        angular.heading_acceleration_state = 1.0f;
        this->elevation_weight = 1.0f;
        this->avoidance_heading = positioner.heading;
        this->avoidance_elevation = positioner.elevation;
        for (std::size_t i = 0; i < breadcrumb_count; ++i) breadcrumbs[i] = positioner.position;
        this->heading_reference = positioner.heading;
        angular.heading_target = blend_angle(this->avoidance_heading, this->heading_reference,
                                             1.0f - angular.heading_acceleration_state);
        this->set_elevation(positioner.elevation, true, angular, positioner.flags);
        angular.reset_heading(true);
        angular.reset_elevation(true);
        this->flags_516 |= 8;
    }
}

void RigPositioning::set_elevation(float elevation, bool avoidance, AngularRigTracking& angular,
                                   std::uint8_t positioner_flags) {
    angular.elevation_target = elevation;
    this->elevation_reference = elevation;
    if (avoidance) {
        this->flags_516 |= std::uint8_t((positioner_flags >> 4) & 1);
        if ((this->flags_516 & 1) != 0) {
            angular.elevation_target =
                blend_angle(angular.elevation_target, this->avoidance_elevation, this->elevation_weight);
        }
    }
    angular.elevation_target = rig_tracking::wrap_vmx(angular.elevation_target);
}

}  // namespace s3::camera
