// Port of skate-core/src/camera/manager_blends.rs.
#include "s3/core/camera/manager_blends.hpp"

#include "s3/core/camera/vector_tracker.hpp"

namespace s3::camera::manager_blends {

using manager_state::clamp;
using manager_state::DEGREES;
using vector_tracker::dot;

float BlendEnvironment::raw_blend_value(std::uint32_t kind, float authored, float dt) {
    const ManagerSubject& s = *this->subject;
    const ManagerState& m = *this->state;
    switch (kind) {
        case 0: return authored;
        case 1: return rs::abs(dot(this->rig->fields.velocity, s.rig.transform[2]));
        case 2: return dot(this->rig->fields.acceleration, this->rig_forward);
        case 3: return this->heading_multiplier * (-this->angular_velocity[1]);
        case 4: return m.apex_height;
        case 5: return this->air_progress();
        case 6: return m.velocity_incline * DEGREES;
        case 7: return this->previous_distance;
        case 8: return this->apex_progress();
        case 9: return m.absolute_velocity_incline * DEGREES;
        case 10: return m.landing_incline * DEGREES;
        case 11: return m.mirrored() ? -s.steering[1] : s.steering[1];
        case 12: return s.steering_for_blend(m.mirrored());
        case 13: return dt == 0.0f ? 0.0f : s.steering_for_turn(m.mirrored());
        case 14: return dt == 0.0f ? 0.0f : rs::f32::MAX;
        case 15: return m.frontside_angle;
        case 16: return s.valid_trajectory_duration();
        case 17: return s.look[0];
        case 18: return s.look[1];
        case 19: return s.look_heading();
        case 20: return m.direction_incline * DEGREES;
        case 21: return s.value_512;
        case 22: return -s.value_516;
        case 23: return m.effect_weight;
        default: return 0.5f;
    }
}

float BlendEnvironment::air_progress() const {
    const ManagerSubject& s = *this->subject;
    if (s.is_ground_camera(this->rig->fields.height_mode)) {
        return 1.0f;
    }
    if (this->state->apex_height <= 0.0f) {
        return 0.0f;
    }
    const float remaining = s.trajectory_duration - s.trajectory_time;
    return clamp(s.trajectory_duration <= 0.0f ? 1.0f : (s.trajectory_duration - remaining) / s.trajectory_duration,
                 0.0f, 1.0f);
}

float BlendEnvironment::apex_progress() const {
    const ManagerSubject& s = *this->subject;
    const ManagerState& m = *this->state;
    if (s.is_ground_camera(this->rig->fields.height_mode)) {
        return (m.flags & 0x40) != 0 ? 0.0f : 1.0f;
    }
    if (m.apex_height <= 0.0f || s.valid_trajectory_duration() < m.apex_time) {
        return 0.0f;
    }
    const float twice = m.apex_time * 2.0f;
    const float remaining = s.valid_trajectory_duration() - twice;
    if (s.trajectory_time > twice) {
        return clamp((s.trajectory_time - twice) / remaining + 1.0f, 1.0f, 2.0f);
    }
    return s.trajectory_time / twice;
}

}  // namespace s3::camera::manager_blends
