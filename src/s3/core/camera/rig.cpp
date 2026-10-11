// Port of skate-core/src/camera/rig.rs.
#include "s3/core/camera/rig.hpp"

namespace s3::camera {
namespace {

float floor_zero(float value) { return -value >= 0.0f ? 0.0f : value; }

}  // namespace

F4 Breadcrumbs::last() const {
    return this->positions[(this->positions.size() + this->index - 1) % this->positions.size()];
}

void Breadcrumbs::update(F4 position) {
    const F4 delta = rs::from_fn<4>([&](std::size_t i) { return position[i] - this->positions[this->index][i]; });
    if (vector_tracker::dot(delta, delta) > this->distance_squared_threshold) {
        this->index += 1;
        if (this->index >= this->positions.size()) {
            this->index = 0;
        }
        this->positions[this->index] = position;
    }
}

void Rig::update(float dt, const RigSettings& settings, RigSubject& subject,
                 std::array<TrajectoryCollisionRequest*, 3>& requests, MovingObstacleProvider& moving,
                 PositionerCollisionProvider& collision) {
    this->fields.reset_time = floor_zero(this->fields.reset_time - dt);
    this->fields.disable_avoidance_time = floor_zero(this->fields.disable_avoidance_time - dt);
    this->fields.frame_counter = this->fields.frame_counter + 1u;
    RigMode mode{this->fields.height_mode, this->fields.previous_height_mode, this->fields.mode_time};
    mode.update(static_cast<RigModeSubject&>(subject));
    this->fields.height_mode = mode.current;
    this->fields.previous_height_mode = mode.previous;
    this->fields.mode_time = mode.elapsed;
    RigFields f = this->fields;
    RigAvoidance avoidance;
    avoidance.anchor = f.anchor;
    avoidance.velocity = f.velocity;
    avoidance.tracked_anchor = this->anchor.position;
    avoidance.heading_target = f.heading_target;
    avoidance.heading_reference = f.heading_reference;
    avoidance.elevation_target = f.elevation_target;
    avoidance.avoidance_heading = f.avoidance_heading;
    avoidance.avoidance_elevation = f.avoidance_elevation;
    avoidance.reset_time = f.reset_time;
    avoidance.disable_time = f.disable_avoidance_time;
    avoidance.height_transition_start = f.height_transition_start;
    avoidance.height_transition_end = f.height_transition_end;
    avoidance.height_transition_duration = f.height_transition_duration;
    avoidance.height_transition_time = f.height_transition_time;
    avoidance.reference_height = f.reference_height;
    avoidance.prediction_distance = f.distance;
    avoidance.heading_weight = f.heading_weight;
    avoidance.elevation_weight = f.elevation_weight;
    avoidance.avoidance_mode = f.avoidance_mode;
    avoidance.height_mode = f.height_mode;
    avoidance.flags_516 = f.flags_516;
    avoidance.paths = this->paths;
    avoidance.update(dt, settings.avoidance, this->positioner, this->breadcrumbs.last(),
                     static_cast<AvoidanceSubject&>(subject), requests, moving);
    this->paths = avoidance.paths;
    this->fields.avoidance_heading = avoidance.avoidance_heading;
    this->fields.avoidance_elevation = avoidance.avoidance_elevation;
    this->fields.heading_weight = avoidance.heading_weight;
    this->fields.elevation_weight = avoidance.elevation_weight;
    this->fields.flags_516 = avoidance.flags_516;
    f = this->fields;
    ReferenceHeightTracking height;
    height.anchor_height = f.anchor[1];
    height.transition_start = f.height_transition_start;
    height.transition_end = f.height_transition_end;
    height.transition_weight = f.height_transition_weight;
    height.transition_duration = f.height_transition_duration;
    height.transition_time = f.height_transition_time;
    height.height = f.reference_height;
    height.previous_height = f.previous_height;
    height.height_offset = f.height_offset;
    height.mode = f.height_mode;
    height.flags_516 = f.flags_516;
    height.flags_517 = f.flags_517;
    height.update(dt, static_cast<ReferenceHeightSubject&>(subject));
    this->fields.height_transition_duration = height.transition_duration;
    this->fields.height_transition_time = height.transition_time;
    this->fields.reference_height = height.height;
    this->fields.previous_height = height.previous_height;
    this->fields.height_offset = height.height_offset;
    AngularRigTracking angular_ = this->angular();
    angular_.update_heading(dt, settings.heading);
    angular_.update_elevation(dt, settings.elevation);
    this->store_angular(angular_);
    f = this->fields;
    const float square = vector_tracker::dot(f.velocity, f.velocity);
    float inverse = physics::reciprocal_sqrt::estimate(square);
    for (int k = 0; k < 2; ++k) {
        inverse = rs::mul_add(inverse * 0.5f, rs::mul_add(-square, inverse * inverse, 1.0f), inverse);
    }
    const float magnitude = square == 0.0f ? 0.0f : square * inverse;
    // Mapped TU3 read-only constant82139A30.
    const F4 fallback = {0.0f, 0.0f, 1.0f, 0.0f};
    const F4 direction = rs::from_fn<4>([&](std::size_t i) {
        return magnitude > settings.normalization_threshold[i] ? f.velocity[i] * inverse : fallback[i];
    });
    const float acceleration = vector_tracker::dot(f.acceleration, direction);
    this->fields.smoothed_acceleration = (f.previous_acceleration + acceleration) * 0.5f;
    this->fields.previous_acceleration = acceleration;
    AnchorRigTracking anchor_tracking;
    anchor_tracking.tracker = this->anchor;
    anchor_tracking.parameters = this->anchor_parameters;
    anchor_tracking.anchor = f.anchor;
    anchor_tracking.velocity = f.velocity;
    anchor_tracking.reference_height = f.reference_height;
    anchor_tracking.latch_time = f.latch_time;
    anchor_tracking.input_time = f.input_time;
    anchor_tracking.flags_516 = f.flags_516;
    anchor_tracking.flags_517 = f.flags_517;
    anchor_tracking.update(dt, settings.anchor, static_cast<AnchorTrackingSubject&>(subject));
    this->anchor = anchor_tracking.tracker;
    this->anchor_parameters = anchor_tracking.parameters;
    this->fields.latch_time = anchor_tracking.latch_time;
    this->fields.input_time = anchor_tracking.input_time;
    this->fields.flags_516 = anchor_tracking.flags_516;
    this->fields.collision_hold_time = (this->positioner.flags & 0x80) != 0
                                           ? settings.collision_hold_duration
                                           : floor_zero(this->fields.collision_hold_time - dt);
    F4 breadcrumb = this->anchor.position;
    breadcrumb[1] = this->fields.reference_height;
    this->breadcrumbs.update(breadcrumb);
    RigFraming framing_ = this->framing();
    angular_ = this->angular();
    framing_.update(dt, angular_, this->anchor.position, this->positioner, settings.normalization_threshold);
    this->fields.anchor = framing_.anchor;
    this->fields.distance = framing_.distance;
    f = this->fields;
    RigPositioning positioning;
    positioning.heading_reference = f.heading_reference;
    positioning.elevation_reference = f.elevation_reference;
    positioning.avoidance_heading = f.avoidance_heading;
    positioning.avoidance_elevation = f.avoidance_elevation;
    positioning.elevation_weight = f.elevation_weight;
    positioning.collision_mode = f.collision_mode;
    positioning.flags_516 = f.flags_516;
    positioning.update(dt, settings.positioning, angular_, framing_, this->anchor.position, this->positioner,
                       this->breadcrumbs.positions.data(), this->breadcrumbs.positions.size(),
                       static_cast<RigPositioningSubject&>(subject), collision);
    this->store_angular(angular_);
    this->fields.heading_reference = positioning.heading_reference;
    this->fields.elevation_reference = positioning.elevation_reference;
    this->fields.avoidance_heading = positioning.avoidance_heading;
    this->fields.avoidance_elevation = positioning.avoidance_elevation;
    this->fields.elevation_weight = positioning.elevation_weight;
    this->fields.flags_516 = positioning.flags_516;
}

AngularRigTracking Rig::angular() const {
    const RigFields& f = this->fields;
    AngularRigTracking a;
    a.heading = this->heading;
    a.heading_parameters = this->heading_parameters;
    a.elevation = this->elevation;
    a.elevation_parameters = this->elevation_parameters;
    a.heading_target = f.heading_target;
    a.elevation_target = f.elevation_target;
    a.heading_offset = f.heading_offset;
    a.reset_time = f.reset_time;
    a.heading_smoothing = f.heading_smoothing;
    a.elevation_smoothing = f.elevation_smoothing;
    a.heading_acceleration_state = f.heading_weight;
    a.flags_517 = f.flags_517;
    return a;
}

void Rig::store_angular(const AngularRigTracking& a) {
    this->heading = a.heading;
    this->heading_parameters = a.heading_parameters;
    this->elevation = a.elevation;
    this->elevation_parameters = a.elevation_parameters;
    this->fields.heading_target = a.heading_target;
    this->fields.elevation_target = a.elevation_target;
    this->fields.heading_weight = a.heading_acceleration_state;
    this->fields.flags_517 = a.flags_517;
}

RigFraming Rig::framing() const {
    const RigFields& f = this->fields;
    RigFraming r;
    r.anchor = f.anchor;
    r.offset = f.offset;
    r.elevation_offset = f.elevation_offset;
    r.additional_elevation = f.additional_elevation;
    r.reference_height = f.reference_height;
    r.distance = f.distance;
    return r;
}

}  // namespace s3::camera
