// Port of skate-core/src/camera/manager_subject.rs.
//! CameraMan inputs published by the normal subject, TU3 82DF69C0.
//! The physics host supplies these independently produced values; camera code
//! does not replace trajectory, stance or animation outputs with pose guesses.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/camera/_types.hpp"
#include "s3/core/camera/anchors.hpp"
#include "s3/core/camera/subject.hpp"

namespace s3::camera {

struct ManagerSubject {
    Subject rig;
    std::array<AnchorState, 7> anchors{};
    /// Subject compass getter364, filled by82DF7B90.
    std::array<float, 9> compass{};
    F4 board_offset_direction{};  // 456
    F4 ground_normal{};           // 448
    F4 launch_position{};         // 400
    F4 launch_normal{};           // 404
    F4 landing_position{};        // 408
    F4 landing_normal{};          // 412
    F4 apex_position{};           // 416
    F4 direction_424{};
    std::array<float, 2> look{};  // 472
    /// Getters484,488,492,496 are distinct source producers.
    F4 steering{};
    float trajectory_time = 0.0f;      // 500
    float trajectory_duration = 0.0f;  // 504
    float apex_time = 0.0f;            // 508
    float value_512 = 0.0f;
    float value_516 = 0.0f;
    std::uint8_t reset = 0;  // 544
    std::uint8_t flag_556 = 0;
    std::uint8_t stance_560 = 0;
    std::uint8_t stance_592 = 0;
    std::uint8_t flag_652 = 0;
    std::uint8_t shake_variant = 0;   // 672
    std::uint8_t special_effect = 0;  // 676
    std::uint8_t flag_684 = 0;

    ///82E00040: this camera predicate is distinct from grounded physics.
    bool is_ground_camera(std::uint32_t height_mode) const;
    ///82E00980. Sign disagreement retains the primary steering channel;
    /// agreement selects the greater magnitude. Equality preserves492.
    float steering_for_turn(bool mirrored) const;
    ///82E00790. This variant starts with channel488 and keeps the secondary
    /// channel on sign disagreement; it is not identical to82E00980.
    float steering_for_blend(bool mirrored) const;
    ///82E00EB0.
    float valid_trajectory_duration() const;
    ///82E00F78; normalize before the native direction-to-angles helper.
    float look_heading() const;
};
S3_DERIVE_EQ(ManagerSubject, rig, anchors, compass, board_offset_direction, ground_normal, launch_position,
             launch_normal, landing_position, landing_normal, apex_position, direction_424, look, steering,
             trajectory_time, trajectory_duration, apex_time, value_512, value_516, reset, flag_556, stance_560,
             stance_592, flag_652, shake_variant, special_effect, flag_684)

}  // namespace s3::camera
