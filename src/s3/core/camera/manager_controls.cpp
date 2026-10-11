// Port of skate-core/src/camera/manager_controls.rs.
//! Normal camera state timing82DFEAA8 and stance tracking82E00108.
#include "s3/core/camera/manager_state.hpp"
#include "s3/core/camera/manager_subject.hpp"
#include "s3/core/camera/rig.hpp"
#include "s3/core/camera/shot_manager.hpp"

namespace s3::camera {

using manager_state::mirror_parameters;
using manager_state::STEP;

void ManagerState::update_timing(const ManagerSubject& subject, float steering_threshold) {
    this->time_without_trajectory = subject.rig.trajectory_valid != 0 ? 0.0f : this->time_without_trajectory + STEP;
    if (subject.reset != 0 || this->reset_requested()) {
        this->options |= 0x80;
        this->flags &= std::uint8_t(~4u);
        this->trajectory_camera_distance = 0.0f;
        this->centred_time = 0.0f;
        this->steering_time = 0.0f;
        this->collision_elevation = 0.0f;
        this->instant_frames = 3;
        this->anchor_velocity = F4{};
    }
    this->steering_time = rs::abs(subject.steering[0]) <= steering_threshold ? 0.0f : this->steering_time + STEP;
    this->centred_time = rs::abs(subject.steering_for_turn(this->mirrored())) >= steering_threshold
                             ? 0.0f
                             : this->centred_time + STEP;
    this->flags = std::uint8_t((this->flags & ~8u) | ((subject.stance_592 != subject.stance_560 ? 1u : 0u) << 3));
}

void ManagerState::update_mirrors(float dt, const ManagerSubject& subject, const Rig& rig, const ShotManager& shots) {
    if (!this->reset_requested() && !subject.is_ground_camera(rig.fields.height_mode)) {
        return;
    }
    const std::uint8_t option =
        this->reset_requested() ? shots.first_leaf().mirror_for_stance : shots.current().shot.mirror_for_stance;
    const float target = (option != 0 && this->mirrored()) ? -1.0f : 1.0f;
    if (this->reset_requested()) {
        for (ScalarTracker* tracker : {&this->heading_mirror, &this->framing_mirror}) {
            tracker->target = target;
            tracker->position = target;
            tracker->velocity = 0.0f;
            tracker->acceleration = 0.0f;
        }
    } else {
        this->heading_mirror.update(dt, target, mirror_parameters(100.0f, 500.0f));
        this->framing_mirror.update(dt, target, mirror_parameters(15.0f, 50.0f));
    }
}

}  // namespace s3::camera
