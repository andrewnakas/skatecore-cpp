// Port of skate-core/src/camera/frame_composer.rs.
//! Complete single/transition framing82E01260/82E01740. Camera positioning,
//! reference producers and stock shot selection remain the caller's inputs.
#pragma once
#include <array>

#include "s3/core/camera/_types.hpp"
#include "s3/core/camera/shot.hpp"

namespace s3::camera {

struct FrameSettings {
    float maximum_pitch_degrees = 0.0f;
    float roll_response = 0.0f;
};
S3_DERIVE_EQ(FrameSettings, maximum_pitch_degrees, roll_response)

struct FrameSubject {
    std::array<F4, 10> positions{};
    /// Subject getter456 (the board-offset direction).
    F4 board_offset_direction{};
};
S3_DERIVE_EQ(FrameSubject, positions, board_offset_direction)

struct FrameComposer {
    float roll = 0.0f;
    F4 reference{};
    float yaw = 0.0f;
    float pitch = 0.0f;

    static FrameComposer new_() { return FrameComposer{}; }

    /// The native transition evaluates `to` before `from`: the latter's
    /// reference/yaw/pitch are therefore the retained composer fields.
    F4 compose(float dt, float fraction, float mirror, const Shot& from, const Shot& to, F4 fallback_reference,
               F4 camera_position, const FrameSubject& subject, FrameSettings settings);

    F4 single(const Shot& shot, F4 fallback_reference, F4 camera_position, float mirror, const FrameSubject& subject,
              FrameSettings settings);
};
S3_DERIVE_EQ(FrameComposer, roll, reference, yaw, pitch)

}  // namespace s3::camera
