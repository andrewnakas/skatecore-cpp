// Port of skate-core/src/camera/rig_initialization.rs.
//! State construction82E02738, reset82E02E58 and teleport82E031D8.
#include "s3/core/camera/rig.hpp"

namespace s3::camera {
namespace {

ScalarTracker scalar(float position) { return ScalarTracker{position, position, 0.0f, 0.0f, 0.0f, 0.0f}; }

ScalarTrackerParameters tracker_parameters() {
    ScalarTrackerParameters p;
    p.overshoot_zeroes_velocity = true;
    return p;
}

RigFields initial_fields(std::uint32_t avoidance_mode) {
    RigFields f;
    f.heading_target = rs::from_bits(0x40490fdb);
    f.heading_reference = rs::from_bits(0x40490fdb);
    f.collision_hold_time = 5.0f;
    f.reset_time = 2.0f;
    f.reference_height = 1000.0f;
    f.previous_height = rs::f32::MAX;
    f.distance = 1.0f;
    f.heading_smoothing = 1.0f;
    f.elevation_smoothing = 1.0f;
    f.avoidance_mode = avoidance_mode;
    f.flags_516 = 0x88;
    f.flags_517 = 0x30;
    return f;
}

}  // namespace

Rig Rig::new_(std::uint32_t avoidance_mode) {
    const ScalarTrackerParameters parameters = tracker_parameters();
    Rig rig;
    rig.fields = initial_fields(avoidance_mode);
    rig.heading = AngleTracker{scalar(0.0f)};
    rig.heading_parameters = parameters;
    rig.elevation = AngleTracker{scalar(0.0f)};
    rig.elevation_parameters = parameters;
    rig.anchor = VectorTracker{};
    rig.anchor_parameters = parameters;
    rig.positioner.distance_tracker = scalar(1.0f);
    rig.positioner.tracker_parameters = parameters;
    rig.positioner.collision_clear_time = rs::f32::MAX;
    // Host storage for parameters fully supplied by the first rig update.
    rig.positioner.config = PositionerConfig{};
    rig.positioner.radius = rs::from_bits(0x3e051eb8);
    AvoidancePath path;
    path.prediction_flag = 0;
    path.evaluator.request_state = 0;
    path.evaluator.path = PredictionPath{};
    path.evaluator.acceleration_flag = 0;
    path.evaluator.collision_time = rs::f32::MAX;
    path.evaluator.last_valid_time = rs::f32::MAX;
    path.evaluator.found = 0;
    path.evaluator.result_acceleration_flag = 0;
    path.evaluator.submitted_acceleration_flag = 0;
    path.evaluator.pending_polls = 0;
    rig.paths = {path, path, path};
    rig.breadcrumbs.positions = std::vector<F4>(2, F4{});
    rig.breadcrumbs.index = 0;
    rig.breadcrumbs.distance_squared_threshold = 1.0f;
    rig.reset_angles();
    return rig;
}

void Rig::reset() {
    const RigFields previous = this->fields;
    this->fields = initial_fields(previous.avoidance_mode);
    this->fields.height_transition_weight = previous.height_transition_weight;
    this->fields.flags_517 = std::uint8_t((previous.flags_517 & 0x0f) | 0x30);
    this->breadcrumbs.distance_squared_threshold = 1.0f;
    this->reset_angles();
}

void Rig::teleport() {
    this->fields.avoidance_heading = 0.0f;
    this->fields.avoidance_elevation = 0.0f;
    this->fields.heading_weight = 0.0f;
    this->fields.elevation_weight = 0.0f;
    this->fields.flags_516 &= std::uint8_t(~1u);
    this->reset_angles();
    this->anchor.target = this->fields.anchor;
    this->anchor.position = this->fields.anchor;
    this->anchor.velocity = F4{};
    this->anchor.acceleration = F4{};
    this->fields.reference_height = this->fields.anchor[1];
    this->fields.height_offset = 0.0f;
    //82E03288 clears the preceding frame's world-space shake offset.
    this->fields.offset = F4{};
    this->positioner.distance_tracker.target = this->fields.distance;
    this->positioner.distance_tracker.position = this->fields.distance;
    this->positioner.distance_tracker.velocity = 0.0f;
    this->positioner.distance_tracker.acceleration = 0.0f;
    this->fields.reset_time = rs::from_bits(0x3dcccccd);
    this->fields.disable_avoidance_time = 2.0f;
    this->fields.collision_hold_time = 5.0f;
    this->fields.height_transition_duration = 0.0f;
    this->fields.flags_517 &= std::uint8_t(~0x40u);
    this->fields.previous_acceleration = 0.0f;
    this->fields.flags_516 = std::uint8_t((this->fields.flags_516 & 0xb3) | 8);
    this->fields.input_time = 0.0f;
    this->fields.latch_time = 0.0f;
}

void Rig::set_elevation(float elevation_, bool use_avoidance) {
    this->fields.elevation_target = elevation_;
    this->fields.elevation_reference = elevation_;
    if (use_avoidance) {
        this->fields.flags_516 |= (this->positioner.flags & 0x10) != 0 ? 1 : 0;
        if ((this->fields.flags_516 & 1) != 0) {
            const float delta = rig_tracking::wrap_vmx(this->fields.avoidance_elevation - elevation_);
            this->fields.elevation_target = rig_tracking::wrap_vmx(
                rig_tracking::wrap_vmx(rs::mul_add(delta, this->fields.elevation_weight, elevation_)));
        }
    }
}

void Rig::reset_angles() {
    AngularRigTracking a = this->angular();
    a.reset_heading(true);
    a.reset_elevation(true);
    this->store_angular(a);
}

}  // namespace s3::camera
