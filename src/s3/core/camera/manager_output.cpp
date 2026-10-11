// Port of skate-core/src/camera/manager_output.rs.
//! Final output/shake ordering from82DFFAA4..82DFFF28.
#include "s3/core/camera/manager.hpp"
#include "s3/core/camera/vector_tracker.hpp"

namespace s3::camera {

void CameraMan::finish_frame(float dt, const ManagerSubject& subject, const ShakeSettings& settings,
                             std::array<const ShakeSamples*, 2> samples, bool reset) {
    const math::Basis3 old_basis = this->frame.basis;
    const F4 old_position = this->frame.position;
    const math::Basis3 basis = this->orientation.basis;
    const F4 position = this->rig.positioner.position;
    this->frame.motion(dt, basis, position);
    const bool enabled = subject.rig.wiping_out == 0 || (this->rig.positioner.flags & 0x80) != 0 ||
                         subject.rig.broken_bone_slowmo != 0;
    const F4 velocity = this->rig.fields.velocity;
    const float speed = vector_tracker::length({velocity[0], 0.0f, velocity[2], 0.0f}) *
                        (subject.flag_652 != 0 ? rs::from_bits(0x3f6147ae) : 1.0f);
    const float distance = vector_tracker::length(
        rs::from_fn<4>([&](std::size_t i) { return this->rig.fields.anchor[i] - position[i]; }));
    const math::Basis3 first =
        this->shake[0].update(dt, speed, distance, basis, enabled, *samples[0], settings);
    const math::Basis3 second = this->shake[1].update(dt, subject.shake_variant != 0 ? speed * 50.0f : speed,
                                                      distance, basis, enabled, *samples[1], settings);
    const std::size_t selected = subject.shake_variant != 0 ? 1 : 0;
    this->frame.basis = selected == 0 ? first : second;
    this->frame.position = position;
    this->frame.shake_translation = this->shake[selected].translation;
    //82DFFEC8..82DFFECC publishes ShakeEffect+16 into Rig+320. The
    // positioner consumes this world-space offset on the following tick;
    // it is not an extra translation added by the renderer to this frame.
    this->rig.fields.offset = this->frame.shake_translation;
    this->frame.discontinuity = (this->rig.positioner.flags & 0x40) != 0 || reset;
    if (this->frame.discontinuity) {
        this->frame.previous_basis = this->frame.basis;
        this->frame.previous_position = position;
    } else {
        this->frame.previous_basis = old_basis;
        this->frame.previous_position = old_position;
    }
    this->frame.field_of_view_degrees = this->state.field_of_view;
    this->frame.opacity = this->state.opacity;
    this->frame.blur = this->state.blur;
}

}  // namespace s3::camera
