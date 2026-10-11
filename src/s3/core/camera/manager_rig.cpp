// Port of skate-core/src/camera/manager_rig.rs.
//! CameraMan shot-to-rig binding82DFF23C..82DFF864.
#include "s3/core/camera/direction.hpp"
#include "s3/core/camera/drop_predictor.hpp"
#include "s3/core/camera/manager_state.hpp"
#include "s3/core/camera/manager_subject.hpp"
#include "s3/core/camera/rig.hpp"
#include "s3/core/camera/rig_orientation.hpp"
#include "s3/core/camera/shot.hpp"
#include "s3/core/camera/vector_tracker.hpp"
#include "s3/core/physics/rigid_body.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::camera {

using manager_state::clamp;
using vector_tracker::length;

float ManagerState::bind_shot(const Shot& shot, const ManagerSubject& subject, Rig& rig, RigOrientation& orientation,
                              const DropPredictor& drop, LookInput look, float previous_roll, bool transitioning) {
    const bool reset = this->reset_requested();
    rig.fields.avoidance_mode = (this->options & 0x20) != 0 ? 1u : 0u;
    rig.fields.flags_516 = std::uint8_t((rig.fields.flags_516 & ~0x62u) | ((transitioning ? 1u : 0u) << 5) |
                                        ((unsigned(shot.follow_subject_in_air) << 6) & 0x40u) |
                                        ((unsigned(shot.avoidance_override) << 1) & 2u));
    rig.fields.flags_517 = std::uint8_t((rig.fields.flags_517 & 0x7fu) | (unsigned(subject.stance_560) << 7));
    const math::Basis3 basis = physics::rigid_body::basis_from_quaternion(physics::rigid_body::RetailQuaternion{
        shot.arm_orientation[0], shot.arm_orientation[1], shot.arm_orientation[2], shot.arm_orientation[3]});
    const auto& at = basis.columns[2];
    const F4 angles = direction_to_angles({at[0], at[1], at[2], 0.0f});
    float elevation = -angles[0];
    float heading = angles[1];
    float distance = shot.distance;
    if (subject.special_effect != 0) {
        elevation += rs::from_bits(0x3eb2b8c2);
        const float time = float(this->frames) * rs::from_bits(0x3dd67750);
        const float distance_wave = trigonometry::sin(time * rs::from_bits(0x3d321643));
        const float heading_wave = trigonometry::sin(time * rs::from_bits(0x3c23d70a));
        heading = rs::mul_add(heading_wave, rs::from_bits(0x40278d36), heading);
        distance = (distance_wave + distance) + 1.0f;
    }
    float drop_elevation = 0.0f;
    if ((this->options & 0x40) != 0) {
        const float speed = length(this->anchor_velocity);
        const float minimum = (this->options & 0x10) != 0 ? 0.7f : 0.9f;
        const float maximum = (this->options & 0x10) != 0 ? 0.8f : 1.0f;
        const float scale = clamp(-rs::mul_add(speed - 4.0f, rs::from_bits(0x3cccccd0), -maximum), minimum, maximum);
        drop_elevation = drop.elevation * scale;
        distance = drop.distance_offset + distance;
    }
    float collision_elevation_;
    if (reset) {
        collision_elevation_ = 0.0f;
    } else {
        const float maximum = distance - 2.5f >= 0.0f ? 2.5f : distance;
        const float fraction = clamp((maximum - rig.positioner.available_distance) / maximum, 0.0f, 1.0f);
        const float angle = (subject.rig.off_board != 0 ? 60.0f : 50.0f) * rs::from_bits(0x3c8efa35);
        const float raw_target = rs::mul_add(angle, fraction, -drop_elevation);
        float target = raw_target >= 0.0f ? raw_target : 0.0f;
        if (subject.is_ground_camera(rig.fields.height_mode) && subject.ground_normal[1] < -0.2f) {
            target *= -1.0f;
        }
        const bool outward = (target > 0.0f && this->collision_elevation < target) ||
                             (target < 0.0f && this->collision_elevation > target);
        const float step = angle * (outward ? rs::from_bits(0x3d888889) : rs::from_bits(0x3c23d70a));
        if (rs::abs(target - this->collision_elevation) > step) {
            this->collision_elevation = target >= this->collision_elevation ? this->collision_elevation + step
                                                                            : this->collision_elevation - step;
        } else {
            this->collision_elevation = target;
        }
        collision_elevation_ =
            clamp(this->collision_elevation, -rs::from_bits(0x3fb2b8c2), rs::from_bits(0x3fb2b8c2));
    }
    rig.fields.distance = distance;
    const float turns = rs::floor(rs::mul_add(heading, rs::from_bits(0x3e22f983), 0.5f));
    const float reference = -rs::mul_add(turns, rs::from_bits(0x40c90fdb), -heading);
    rig.fields.heading_reference = reference;
    if (reset) {
        rig.fields.heading_target = reference;
    } else {
        const float from = rig.fields.avoidance_heading;
        const float delta = rig_tracking::wrap_vmx(reference - from);
        rig.fields.heading_target =
            rig_tracking::wrap_vmx(rs::mul_add(delta, 1.0f - rig.fields.heading_weight, from));
    }
    rig.set_elevation(elevation, !reset);
    rig.fields.elevation_offset = drop_elevation + look.elevation;
    rig.fields.additional_elevation = collision_elevation_;
    rig.fields.flags_517 =
        std::uint8_t((rig.fields.flags_517 & ~0x20u) | ((shot.compass_north != 5 ? 1u : 0u) << 5));
    if (reset) {
        this->blur = 0.0f;
    } else if (transitioning) {
        this->blur = shot.transition_blur;
    } else {
        this->blur = shot.blur;
    }
    const float heading_mirror_ = shot.compass_north == 2 ? 1.0f : this->heading_mirror.position;
    orientation.framing_mirror = shot.compass_north == 2 ? 1.0f : this->framing_mirror.position;
    rig.fields.collision_mode = shot.collision_hint;
    const float h = 1.0f - shot.smoothing[0];
    const float e = 1.0f - shot.smoothing[1];
    const float pan = 1.0f - shot.smoothing[2];
    const float tilt = 1.0f - shot.smoothing[3];
    rig.fields.heading_smoothing = 1.0f - (-rs::mul_add(h, h, -1.0f));
    rig.fields.elevation_smoothing = 1.0f - (-rs::mul_add(e, e, -1.0f));
    orientation.pan_smoothing = -rs::mul_add(pan, pan, -1.0f);
    orientation.tilt_smoothing = -rs::mul_add(tilt, tilt, -1.0f);
    orientation.roll = previous_roll;
    return heading_mirror_;
}

}  // namespace s3::camera
