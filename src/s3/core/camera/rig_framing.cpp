// Port of skate-core/src/camera/rig_framing.rs.
#include "s3/core/camera/rig_framing.hpp"

#include "s3/core/camera/direction.hpp"
#include "s3/core/camera/vector_tracker.hpp"

namespace s3::camera {

void RigFraming::update(float dt, AngularRigTracking& angular, F4 tracked_anchor, const Positioner& positioner,
                        F4 normalization_threshold) {
    if ((angular.flags_517 & 0x40) == 0) {
        return;
    }
    F4 camera = positioner.position;
    if ((positioner.flags & 0x80) == 0) {
        camera = rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(positioner.velocity[i], dt, camera[i]); });
    }
    F4 anchor_ = tracked_anchor;
    anchor_[1] = this->reference_height;
    const F4 delta = rs::from_fn<4>([&](std::size_t i) { return (camera[i] - anchor_[i]) - this->offset[i]; });
    const float square = vector_tracker::dot(delta, delta);
    float inverse = physics::reciprocal_sqrt::estimate(square);
    for (int k = 0; k < 2; ++k) {
        inverse = rs::mul_add(inverse * 0.5f, rs::mul_add(-square, inverse * inverse, 1.0f), inverse);
    }
    const float magnitude = square == 0.0f ? 0.0f : square * inverse;
    const F4 direction = rs::from_fn<4>(
        [&](std::size_t i) { return magnitude > normalization_threshold[i] ? delta[i] * inverse : 0.0f; });
    const F4 angles = direction_to_angles(direction);
    angular.heading_target = -angles[1] - angular.heading_offset;
    angular.elevation_target = (angles[0] - this->elevation_offset) - this->additional_elevation;
    this->distance = magnitude;
    this->anchor[1] = this->reference_height;
    angular.reset_heading(false);
    angular.reset_elevation(false);
}

void RigMode::update(RigModeSubject& subject) {
    std::uint32_t next;
    if (subject.flag_568() != 0) {
        next = 0;
    } else if (subject.flag_596() != 0 || subject.flag_680() != 0) {
        next = 2;
    } else if (subject.flag_540() != 0) {
        next = 1;
    } else {
        return;
    }
    if (next != this->current) {
        this->previous = this->current;
        this->current = next;
        this->elapsed = 0.0f;
    }
}

}  // namespace s3::camera
