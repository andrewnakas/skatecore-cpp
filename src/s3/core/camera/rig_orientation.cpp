// Port of skate-core/src/camera/rig_orientation.rs.
#include "s3/core/camera/rig_orientation.hpp"

#include "s3/core/camera/direction.hpp"
#include "s3/core/camera/orientation_math.hpp"
#include "s3/core/camera/rig_tracking.hpp"
#include "s3/core/physics/rigid_body.hpp"

namespace s3::camera {
namespace {

void reset(AngleTracker& tracker, float target) {
    tracker._0.target = normalize_angle(target);
    tracker._0.position = normalize_angle(target);
    tracker._0.velocity = 0.0f;
    tracker._0.acceleration = 0.0f;
}

float cap_smoothing(float value) {
    if (value >= 1.0f) {
        return 1.0f;
    } else if (rs::from_bits(0x3f666666) - value >= 0.0f) {
        return value;
    }
    return rs::from_bits(0x3f666666);
}

void bind(ScalarTrackerParameters& p, const OrientationTrackerSettings& s, float smoothing, bool clamp_acceleration) {
    const float radians = rs::from_bits(0x3c8efa35);
    p.speed_clamp = rs::f32::MAX;
    p.acceleration_clamp_min = clamp_acceleration ? s.acceleration_min_degrees * radians : rs::f32::MAX;
    p.acceleration_clamp_max = clamp_acceleration ? s.acceleration_max_degrees * radians : rs::f32::MAX;
    p.smoothing_min = smoothing < 1.0f ? (s.smoothing_min - smoothing >= 0.0f ? smoothing : s.smoothing_min) : 1.0f;
    p.smoothing_max = smoothing < 1.0f ? smoothing : 1.0f;
    p.delta_umbra = s.delta_umbra_degrees * radians;
    p.delta_penumbra = s.delta_penumbra_degrees * radians;
}

}  // namespace

RigOrientation RigOrientation::new_() {
    RigOrientation o;
    const AngleTracker tracker{ScalarTracker{}};
    ScalarTrackerParameters parameters;
    parameters.overshoot_zeroes_velocity = true;
    o.target = F4{};
    o.basis.columns = {{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
    o.pitch = tracker;
    o.yaw = tracker;
    o.pitch_parameters = parameters;
    o.yaw_parameters = parameters;
    o.pan_smoothing = 0.0f;
    o.tilt_smoothing = 0.0f;
    o.roll = 0.0f;
    o.framing_mirror = 1.0f;
    return o;
}

void RigOrientation::update(float dt, const OrientationSettings& settings, float reset_time,
                            std::uint8_t& flags_516, std::uint8_t flags_517) {
    // The same sqrt(2)-scaled quaternion expansion occurs in the rigid
    // body's basis helper and82E0466C..82E046AC.
    const math::Basis3 target_basis = physics::rigid_body::basis_from_quaternion(
        physics::rigid_body::RetailQuaternion{this->target[0], this->target[1], this->target[2], this->target[3]});
    const auto& at = target_basis.columns[2];
    const F4 angles = direction_to_angles({at[0], at[1], at[2], 0.0f});
    const float pitch_ = -angles[0];
    const float yaw_ = angles[1];
    float error = normalize_angle(normalize_angle(yaw_) - this->yaw._0.position);
    if (error < normalize_angle(0.0f)) {
        error = normalize_angle(normalize_angle(-error));
    }
    const float radians = rs::from_bits(0x3c8efa35);
    const float umbra = settings.pan_umbra_degrees * radians;
    const float penumbra = settings.pan_penumbra_degrees * radians;
    float fraction;
    if (error < umbra) {
        fraction = 1.0f;
    } else if (error < penumbra) {
        fraction = 1.0f - rig_tracking::wrap_vmx(error - umbra) / rig_tracking::wrap_vmx(penumbra - umbra);
    } else {
        fraction = 0.0f;
    }
    const float response = settings.pan_smoothing_curve.evaluate(fraction);
    float minimum = 1.0f - settings.pan_minimum_smoothing;
    minimum = -(minimum * minimum - 1.0f);
    float pan = this->pan_smoothing >= minimum ? (this->pan_smoothing - minimum) * response + minimum
                                               : this->pan_smoothing;
    pan = cap_smoothing(pan);
    this->tilt_smoothing = cap_smoothing(this->tilt_smoothing);
    if ((flags_516 & 8) != 0 || reset_time > 0.0f) {
        reset(this->pitch, pitch_);
        reset(this->yaw, yaw_);
        flags_516 &= std::uint8_t(~8u);
    }
    if ((flags_516 & 0x40) != 0) {
        bind(this->pitch_parameters, settings.tilt, this->tilt_smoothing, (flags_517 & 0x10) != 0);
        this->pitch.update(dt, normalize_angle(pitch_), this->pitch_parameters);
    }
    bind(this->yaw_parameters, settings.pan, pan, true);
    this->yaw.update(dt, normalize_angle(yaw_), this->yaw_parameters);
    const math::Basis3 basis_ =
        orientation_math::basis_from_angles(this->pitch._0.position, this->yaw._0.position, 0.0f);
    this->basis = orientation_math::rotate_about_axis(basis_, basis_.columns[2], this->roll * this->framing_mirror);
}

}  // namespace s3::camera
