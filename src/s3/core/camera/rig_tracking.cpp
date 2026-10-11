// Port of skate-core/src/camera/rig_tracking.rs.
#include "s3/core/camera/rig_tracking.hpp"

namespace s3::camera {
namespace {

void bind_angle_settings(ScalarTrackerParameters& p, AngleTrackingSettings s, float smoothing) {
    const float radians = rs::from_bits(0x3c8efa35);
    p.speed_clamp = s.speed_clamp_degrees * radians;
    p.acceleration_clamp_min = s.acceleration_min_degrees * radians;
    p.acceleration_clamp_max = s.acceleration_max_degrees * radians;
    p.delta_umbra = s.delta_umbra_degrees * radians;
    p.delta_penumbra = s.delta_penumbra_degrees * radians;
    p.smoothing_min = smoothing;
    p.smoothing_max = smoothing;
}

float floor_zero(float value) { return -value >= 0.0f ? 0.0f : value; }
float clamp_fraction(float value) {
    const float v = floor_zero(value);
    return 1.0f - v >= 0.0f ? v : 1.0f;
}

}  // namespace

float AngularRigTracking::current_heading() const {
    return rig_tracking::wrap_vmx(this->heading._0.position + this->heading_offset);
}

void AngularRigTracking::reset_heading(bool clear_motion) {
    this->heading._0.target = normalize_angle(this->heading_target);
    this->heading._0.position = normalize_angle(this->heading_target);
    if (clear_motion) {
        this->heading._0.velocity = 0.0f;
        this->heading._0.acceleration = 0.0f;
    }
}

void AngularRigTracking::reset_elevation(bool clear_motion) {
    this->elevation._0.target = normalize_angle(this->elevation_target);
    this->elevation._0.position = normalize_angle(this->elevation_target);
    if (clear_motion) {
        this->elevation._0.velocity = 0.0f;
        this->elevation._0.acceleration = 0.0f;
    }
}

void AngularRigTracking::update_heading(float dt, AngleTrackingSettings settings) {
    if ((this->flags_517 & 0x20) == 0 && this->heading_acceleration_state == 0.0f) {
        return;
    }
    const float error = rs::abs(rig_tracking::wrap_vmx(this->heading_target - this->current_heading()));
    float smoothing = 1.0f - this->heading_smoothing;
    const float capped = rs::from_bits(0x3f4ccccd) - smoothing >= 0.0f ? smoothing : rs::from_bits(0x3f4ccccd);
    smoothing = error < rs::from_bits(0x3f490fdb)
                    ? rs::mul_add(capped - smoothing, error * rs::from_bits(0x3fa2f984), smoothing)
                    : capped;
    if (this->reset_time > 0.0f) {
        this->reset_heading(true);
    }
    bind_angle_settings(this->heading_parameters, settings, smoothing);
    if (this->heading_acceleration_state > 0.0f) {
        this->heading_parameters.acceleration_clamp_min *= 2.0f;
        this->heading_parameters.acceleration_clamp_max *= 2.0f;
    }
    this->heading_parameters.overshoot_zeroes_velocity = false;
    this->heading.update(dt, normalize_angle(this->heading_target), this->heading_parameters);
}

void AngularRigTracking::update_elevation(float dt, AngleTrackingSettings settings) {
    if (this->reset_time > 0.0f) {
        this->reset_elevation(true);
    }
    bind_angle_settings(this->elevation_parameters, settings, 1.0f - this->elevation_smoothing);
    this->elevation.update(dt, normalize_angle(this->elevation_target), this->elevation_parameters);
}

namespace rig_tracking {
float wrap_vmx(float angle) {
    const float tau = rs::from_bits(0x40c90fdb);
    const float pi = rs::from_bits(0x40490fdb);
    const float turns = rs::trunc(vector_tracker::refined_reciprocal(tau) * angle);
    const float reduced = rs::mul_add(-turns, tau, angle);
    const float lower = reduced + pi >= 0.0f ? reduced : reduced + tau;
    return reduced - pi >= 0.0f ? reduced - tau : lower;
}
}  // namespace rig_tracking

float AnchorRigTracking::latch_smoothing(float dt, const AnchorTrackingSettings& s, AnchorTrackingSubject& subject) {
    this->latch_time = floor_zero(this->latch_time - dt);
    if (subject.flag_584() == 0) {
        this->flags_516 &= std::uint8_t(~4u);
    } else if (subject.flag_588() != 0) {
        this->flags_516 |= 4;
    }
    if ((this->flags_516 & 4) != 0) {
        this->latch_time = 0.5f;
    }
    return clamp_fraction(s.latch_curve.evaluate(rs::mul_add(-this->latch_time, 2.0f, 1.0f))) * s.latch_scale;
}

float AnchorRigTracking::input_smoothing(float dt, const AnchorTrackingSettings& s, AnchorTrackingSubject& subject) {
    this->input_time = floor_zero(this->input_time - dt);
    if (subject.value_476() > s.input_threshold) {
        this->input_time = 0.5f;
    }
    return clamp_fraction(s.input_curve.evaluate(rs::mul_add(-this->input_time, 2.0f, 1.0f))) * s.input_scale;
}

void AnchorRigTracking::update(float dt, const AnchorTrackingSettings& s, AnchorTrackingSubject& subject) {
    const float latch = this->latch_smoothing(dt, s, subject);
    const float input = this->input_smoothing(dt, s, subject);
    float smoothing;
    if (subject.flag_548() != 0 || (this->flags_517 & 0x40) != 0) {
        smoothing = 0.0f;
    } else {
        const float maximum = latch - input >= 0.0f ? latch : input;
        smoothing = maximum * clamp_fraction((vector_tracker::length(this->velocity) - 8.0f) * -0.25f);
    }
    this->parameters.speed_clamp = s.speed_clamp;
    this->parameters.acceleration_clamp_min = s.acceleration_clamp;
    this->parameters.acceleration_clamp_max = s.acceleration_clamp;
    this->parameters.smoothing_min = smoothing;
    this->parameters.smoothing_max = smoothing;
    F4 target = this->anchor;
    target[1] = this->reference_height;
    this->tracker.update(dt, target, this->parameters);
}

void ReferenceHeightTracking::update(float dt, ReferenceHeightSubject& subject) {
    const bool immediate = (this->flags_516 & 0x40) != 0;
    if (immediate) {
        this->transition_duration = 0.0f;
    }
    const float old_height = this->height;
    float target = old_height;
    const bool blending = !immediate && this->transition_time < this->transition_duration;
    if (blending) {
        const float fraction = clamp_fraction(this->transition_time / this->transition_duration);
        target = rs::mul_add(fraction, this->transition_end - this->transition_start, this->transition_start);
        if ((this->flags_517 & 8) != 0) {
            const float weight = this->transition_weight * this->transition_weight;
            target = rs::mul_add(1.0f - weight, target, this->anchor_height * weight);
        }
    } else if (this->mode == 0 || this->mode == 2) {
        this->previous_height = old_height;
        target = this->anchor_height;
    } else if (immediate || old_height > this->anchor_height) {
        target = this->anchor_height;
    }
    float smoothing = 1.0f;
    if (old_height != rs::f32::MAX && !blending && this->mode != 2 && subject.flag_656() != 0) {
        smoothing = rs::from_bits(0x3c23d70a);
    }
    this->height = rs::mul_add(target - this->height, smoothing, this->height);
    if (subject.flag_608() != 0) {
        const float delta = this->height - this->anchor_height;
        if (rs::abs(delta) > rs::from_bits(0x38d1b717)) {
            this->height_offset = delta;
            if (delta > 0.0f) {
                this->height = this->anchor_height;
                this->height_offset = 0.0f;
            }
        } else if (rs::abs(this->height_offset) > rs::from_bits(0x3ba3d70a)) {
            this->height_offset *= rs::from_bits(0x3f666666);
            this->height += this->height_offset;
        } else {
            this->height_offset = 0.0f;
        }
    } else {
        this->height_offset = this->height - this->anchor_height;
    }
    this->transition_time += dt;
}

}  // namespace s3::camera
