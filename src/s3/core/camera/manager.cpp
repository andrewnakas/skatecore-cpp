// Port of skate-core/src/camera/manager.rs.
#include "s3/core/camera/manager.hpp"

#include "s3/core/camera/manager_blends.hpp"

namespace s3::camera {

using manager_blends::BlendEnvironment;

CameraMan CameraMan::new_() {
    CameraMan m{ManagerState::new_(), Rig::new_(1), ShotManager::new_(), RigOrientation::new_(),
                FrameComposer::new_(), DropPredictor::new_(), LookInput::new_(),
                {ShakeEffect::new_(), ShakeEffect::new_()}, CameraFrame::new_(), F4{0.0f, 0.0f, 1.0f, 0.0f}, 1.0f};
    return m;
}

void CameraMan::prepare(const ManagerSubject& subject, const ManagerSettings& settings) {
    this->state.update_timing(subject, settings.steering_threshold);
    if (auto impulse = this->state.update_air(subject, this->rig)) {
        this->shake[0].landing_impulse(impulse->first, impulse->second, settings.shake);
    }
    this->state.update_incline(subject);
}

rs::Result<bool, std::string> CameraMan::set_shot(const std::string& name, bool force, const ManagerSubject& subject,
                                                  const ShotDatabase& database) {
    BlendEnvironment environment(this->state, subject, this->rig, this->subject_forward, F4{},
                                 this->heading_multiplier, this->shots.previous.distance);
    return this->shots.set_shot(name, force, database, environment,
                                ShotPlacement{
                                    this->rig.fields.anchor,
                                    this->rig.positioner.position,
                                    this->rig.positioner.config.clamp_height,
                                    this->rig.positioner.config.floor_height,
                                });
}

rs::Result<CameraFrame, std::string> CameraMan::update(float dt, ManagerSubject& subject,
                                                       const ManagerSettings& settings,
                                                       std::array<const ShakeSamples*, 2> samples,
                                                       std::array<TrajectoryCollisionRequest*, 3>& requests,
                                                       MovingObstacleProvider& moving, CollisionProvider& collision) {
    if (this->shots.current().name.empty()) {
        return rs::make_err(std::string("The stock camera graph has not selected a shot"));
    }
    this->state.frames = this->state.frames + 1u;
    if (this->state.frames > 30) {
        this->state.flags |= 1;
    }
    this->state.update_mirrors(dt, subject, this->rig, this->shots);
    const bool reset = this->state.reset_requested();
    if ((this->state.options & 0x40) != 0) {
        if (reset) {
            this->drop.reset();
        }
        const F4 velocity = DropPredictor::prediction_velocity(reset, subject.rig.transform,
                                                               this->rig.fields.velocity, settings.drop);
        S3_CAM_TRY(rs::Unit ignored,
                   this->drop.update(subject.rig.transform[3], velocity,
                                     this->shots.current().shot.use_drop_predictor != 0 && !reset,
                                     subject.rig.off_board != 0, subject.rig.context, settings.drop,
                                     static_cast<DropCollisionProvider&>(collision)));
        (void)ignored;
    }
    const bool look_enabled =
        (reset ? this->shots.first_leaf().use_free_camera_stick : this->shots.current().shot.use_free_camera_stick) !=
        0;
    this->rig.fields.flags_517 =
        std::uint8_t((this->rig.fields.flags_517 & ~0x10u) | ((look_enabled ? 0u : 1u) << 4));
    this->look.update(look_enabled ? subject.look : std::array<float, 2>{0.0f, 0.0f}, settings.look);

    if (reset) {
        this->shots.make_instant();
        this->rig.fields.velocity = F4{};
        this->rig.fields.acceleration = F4{};
        this->state.slowmo_frames = 0;
        this->shake = {ShakeEffect::new_(), ShakeEffect::new_()};
        this->heading_multiplier = this->state.heading_mirror.position;
        this->orientation.framing_mirror = this->state.framing_mirror.position;
    } else {
        this->state.slowmo_frames = subject.rig.broken_bone_slowmo != 0 ? this->state.slowmo_frames + 1u : 0u;
        if (this->state.slowmo_frames == 1) {
            const float impulse = settings.shake.impulse_magnitude * 20.0f;
            if (impulse > this->shake[0].impulse) {
                this->shake[0].impulse = impulse;
            }
        }
    }
    {
        BlendEnvironment environment(this->state, subject, this->rig, this->subject_forward, F4{},
                                     this->heading_multiplier, this->shots.previous.distance);
        this->shots.update(dt, environment);
    }
    this->bind_anchor(subject, reset);
    this->heading_multiplier =
        this->state.bind_shot(this->shots.interpolated, subject, this->rig, this->orientation, this->drop, this->look,
                              this->composer.roll, this->shots.is_transitioning());
    if (reset) {
        this->rig.teleport();
        this->frame.shake_translation = F4{};
    }
    this->rig.update(dt, settings.rig, subject.rig, requests, moving,
                     static_cast<PositionerCollisionProvider&>(collision));
    this->state.opacity = this->shots.interpolated.subject_opacity;
    this->state.field_of_view =
        manager_frame::lens_field_of_view(this->shots.interpolated.lens_length, this->state.aspect_ratio);
    this->orientation.target = this->composer.compose(
        dt, this->shots.framing_fraction(), this->orientation.framing_mirror, this->shots.previous,
        this->shots.current().shot, this->rig.anchor.position, this->rig.positioner.position,
        FrameSubject{subject.rig.reference_positions, subject.board_offset_direction}, settings.framing);
    this->orientation.update(dt, settings.orientation, this->rig.fields.reset_time, this->rig.fields.flags_516,
                             this->rig.fields.flags_517);
    this->finish_frame(dt, subject, settings.shake, samples, reset);
    this->shots.instant_changes = reset || this->state.instant_frames > 0;
    if (reset) {
        this->state.options &= std::uint8_t(~0x80u);
        this->state.flags |= 2;
    } else if (subject.is_ground_camera(this->rig.fields.height_mode)) {
        this->state.flags &= std::uint8_t(~2u);
    }
    if (this->state.instant_frames > 0) {
        this->state.instant_frames -= 1;
    }
    return this->frame;
}

void CameraMan::bind_anchor(const ManagerSubject& subject, bool reset) {
    const std::uint32_t anchor = reset ? this->shots.interpolated.anchor : this->shots.current().shot.anchor;
    if (!reset && anchor != this->state.selected_anchor) {
        this->rig.fields.flags_517 |= 0x40;
    }
    this->state.selected_anchor = anchor;
    const AnchorState selected = subject.anchors[anchor];
    this->state.anchor = selected.position;
    this->state.anchor_velocity = reset ? F4{} : selected.velocity;
    // Native setter subtracts velocities without division by the timestep.
    this->rig.fields.acceleration = rs::from_fn<4>(
        [&](std::size_t i) { return this->state.anchor_velocity[i] - this->rig.fields.velocity[i]; });
    this->rig.fields.velocity = this->state.anchor_velocity;
    this->rig.fields.anchor = this->state.anchor;
    this->subject_forward = subject.rig.transform[2];
    this->rig.fields.flags_516 |= 0x10;
    this->composer.reference = reset ? this->state.anchor : this->rig.anchor.position;
}

}  // namespace s3::camera
