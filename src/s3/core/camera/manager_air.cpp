// Port of skate-core/src/camera/manager_air.rs.
//! Camera trajectory transitions82DFE0E0 and slope relation82DFE718.
#include "s3/core/camera/angle_tracker.hpp"
#include "s3/core/camera/direction.hpp"
#include "s3/core/camera/manager_state.hpp"
#include "s3/core/camera/manager_subject.hpp"
#include "s3/core/camera/rig.hpp"
#include "s3/core/camera/vector_tracker.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::camera {

using manager_state::clamp;
using vector_tracker::dot;
using vector_tracker::length;

std::optional<std::pair<F4, F4>> ManagerState::update_air(const ManagerSubject& subject, Rig& rig) {
    const float previous_delta = this->landing_height_delta;
    this->landing_height_delta = subject.landing_position[1] - subject.launch_position[1];
    this->apex_height = subject.apex_position[1] - subject.launch_position[1];
    this->apex_time = subject.apex_time;
    this->launch_position = subject.launch_position;
    this->apex_height_ratio =
        this->trajectory_camera_distance != 0.0f ? this->apex_height / this->trajectory_camera_distance : 0.0f;
    this->landing_normal = subject.landing_normal;
    this->launch_incline = trigonometry::acos(clamp(subject.launch_normal[1], -1.0f, 1.0f));
    this->flags &= std::uint8_t(~0x10u);
    const bool ground = subject.is_ground_camera(rig.fields.height_mode);
    if (ground || subject.rig.trajectory_valid == 0) {
        this->flags &= std::uint8_t(~4u);
    } else {
        if ((this->flags & 4) != 0) {
            if (rs::abs(this->landing_height_delta - previous_delta) > rs::from_bits(0x3a83126f)) {
                rig.fields.flags_517 |= 8;
            }
        } else {
            const F4 selected = subject.anchors[this->selected_anchor].position;
            this->trajectory_camera_distance =
                length(rs::from_fn<4>([&](std::size_t i) { return rig.positioner.position[i] - selected[i]; }));
            rig.fields.flags_517 &= std::uint8_t(~8u);
        }
        this->flags |= 4;
        this->landing_incline = trigonometry::acos(clamp(this->landing_normal[1], -1.0f, 1.0f));
        this->signed_landing_incline = this->landing_incline;
        if (dot(subject.rig.transform[2], this->landing_normal) > 0.0f) {
            this->signed_landing_incline = -this->landing_incline;
        }
        if (this->launch_incline > rs::from_bits(0x3f9c61aa) && this->landing_incline > rs::from_bits(0x3f060a92)) {
            this->flags |= 0x10;
        }
        this->update_slope_relation(subject);
    }
    rig.fields.height_transition_weight =
        (!ground && subject.rig.trajectory_valid != 0)
            ? clamp(subject.trajectory_time / subject.trajectory_duration, 0.0f, 1.0f)
            : 0.0f;
    std::optional<std::pair<F4, F4>> impulse;
    if (ground) {
        if ((this->flags & 0x40) == 0 && (rig.positioner.flags & 0x80) == 0) {
            if (dot(this->previous_velocity, this->ground_normal) < -0.0f) {
                impulse = std::make_pair(this->ground_normal, this->previous_velocity);
            }
            this->landing_height_delta = 0.0f;
        }
    } else if (rig.fields.height_transition_time >= rig.fields.height_transition_duration &&
               subject.rig.trajectory_valid != 0 && this->landing_height_delta > -3.0f && subject.flag_684 == 0) {
        if ((this->flags & 2) != 0 || subject.rig.air_flag_452 != 0) {
            rig.fields.flags_516 |= 0x40;
        } else if (rig.fields.previous_height != rs::f32::MAX && rig.fields.reference_height != rs::f32::MAX) {
            rig.fields.height_transition_start = rig.fields.reference_height;
            rig.fields.height_transition_end = this->landing_height_delta + rig.fields.reference_height;
            rig.fields.height_transition_duration = subject.trajectory_duration - subject.trajectory_time;
            rig.fields.height_transition_time = 0.0f;
        }
    }
    this->flags = std::uint8_t((this->flags & ~0x40u) | (ground ? 0x40u : 0u));
    this->previous_velocity = rig.fields.velocity;
    return impulse;
}

void ManagerState::update_slope_relation(const ManagerSubject& subject) {
    this->launch_landing_heading_delta = rs::from_bits(0x40490fdb);
    if (subject.launch_normal[1] < rs::from_bits(0x3f7d70a4) && subject.landing_normal[1] < rs::from_bits(0x3f7d70a4)) {
        const float launch = normalize_angle(direction_to_angles(subject.launch_normal)[1]);
        const float landing = normalize_angle(direction_to_angles(subject.landing_normal)[1]);
        this->launch_landing_heading_delta = rs::abs(normalize_angle(landing - launch));
    }
}

}  // namespace s3::camera
