// Port of skate-core/src/riding/grounded/state/output.rs.
// `PhysState_PhysicsGround::FillPhysOut` (`82D3A388`).
//
// The TU3 body is a leaf: it reads Ground and ProcessedPhysIn fields and
// writes distinct PhysOut records. Names recovered from Skate 2 are used only
// where the TU3 offsets and operations agree. Remaining fields retain their
// destination offsets. Xenon vector arithmetic uses the project's isolated
// accepted host approximation; no generated runtime code is included.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/riding/grounded/state/data.hpp"

namespace s3::riding::grounded::state::output {

using data::PhysicsGroundState;

using Vector4 = std::array<float, 4>;

/// ProcessedPhysIn values consumed by the TU3 output leaf.
struct GroundOutputFrame {
    Vector4 axis_464{};
    Vector4 velocity_608{};
    float absolute_body_speed_2616 = 0.0f;
    float deck_speed_2652 = 0.0f;
    float state_timer_2664 = 0.0f;
    float scalar_2720 = 0.0f;
    std::uint32_t flags_2476 = 0;
    std::uint32_t flags_2484 = 0;
    /// ProcessedPhysIn+2548 -> selected collection+4 -> byte+109.
    bool selected_mode_flag_109 = false;
};
S3_DERIVE_EQ(GroundOutputFrame, axis_464, velocity_608, absolute_body_speed_2616, deck_speed_2652, state_timer_2664,
             scalar_2720, flags_2476, flags_2484, selected_mode_flag_109)

/// Live collection values loaded through the global collection singleton.
struct GroundOutputSettings {
    /// Selected ground-layout +4 and +8, added before the pushable-speed test.
    std::array<float, 2> pushable_speed_terms_4_8{};
    /// Selected ground-layout +0, used by the conditional output+52/+58 test.
    float mode_speed_threshold_0 = 0.0f;
};
S3_DERIVE_EQ(GroundOutputSettings, pushable_speed_terms_4_8, mode_speed_threshold_0)

struct SkateboardMotionOutput {
    /// PhysOut pointer+4, byte+274.
    bool is_push_accelerating = false;
    /// PhysOut pointer+4, byte+276.
    bool is_at_pushable_speed = false;
};
S3_DERIVE_EQ(SkateboardMotionOutput, is_push_accelerating, is_at_pushable_speed)

/// Conditional writes to the record reached through PhysOut pointer+36.
struct GroundVelocityProjectionOutput {
    /// Destination +144.
    Vector4 velocity_without_axis_component{};
    /// Destination +164. The native branch only ever writes true.
    bool active = false;
};
S3_DERIVE_EQ(GroundVelocityProjectionOutput, velocity_without_axis_component, active)

struct GroundRecordOutput {
    /// PhysOut pointer+32, bytes +316/+317/+318.
    bool wall_ride_exit = false;
    bool anti_flip_nudge_present = false;
    bool is_pinning = false;
    /// PhysOut pointer+32, vector+128.
    Vector4 anti_flip_torque{};
    /// PhysOut pointer+32, floats +276/+280.
    float time_to_skitch = 0.0f;
    float skitch_spline_height = 0.0f;
    /// PhysOut pointer+32, float+304.
    float processed_scalar_2720 = 0.0f;
    /// PhysOut pointer+32, byte+325.
    bool processed_flag_2484_bit_13 = false;
};
S3_DERIVE_EQ(GroundRecordOutput, wall_ride_exit, anti_flip_nudge_present, is_pinning, anti_flip_torque,
             time_to_skitch, skitch_spline_height, processed_scalar_2720, processed_flag_2484_bit_13)

struct StateRecordOutput {
    /// PhysOut pointer+28, words +36/+40.
    std::uint32_t grab_spline_type = 0;
    std::uint32_t grab_spline_object_id = 0;
    /// PhysOut pointer+28, byte+84. Stock name is not yet proven.
    bool flag_84 = false;
    /// PhysOut pointer+28, byte+86.
    bool has_world_grab_intent_without_object = false;
    /// PhysOut pointer+28, byte+78 is untouched when this is `None`.
    std::optional<bool> manual_correction_write_78;
};
S3_DERIVE_EQ(StateRecordOutput, grab_spline_type, grab_spline_object_id, flag_84,
             has_world_grab_intent_without_object, manual_correction_write_78)

struct IntentRecordOutput {
    /// PhysOut pointer+52, byte+55.
    bool has_world_grab_intent = false;
    /// PhysOut pointer+52, byte+58. Stock name is not yet proven.
    bool selected_mode_below_speed_threshold_58 = false;
};
S3_DERIVE_EQ(IntentRecordOutput, has_world_grab_intent, selected_mode_below_speed_threshold_58)

/// All writes made by TU3 `82D3A388`. Field grouping follows the pointer table
/// reached through the PhysOut argument.
struct PhysicsGroundOutput {
    SkateboardMotionOutput skateboard_motion_4;
    /// `None` means the native `ProcessedPhysIn+2664 == 0.0` branch did not
    /// touch pointer+36 vector+144 or byte+164.
    std::optional<GroundVelocityProjectionOutput> velocity_projection_36;
    GroundRecordOutput ground_32;
    StateRecordOutput state_28;
    IntentRecordOutput intents_52;
    /// PhysOut pointer+72, byte+304.
    bool is_grabbing_object_72_304 = false;
    /// PhysOut pointer+56, byte+168.
    bool manual_opposition_56_168 = false;
    /// PhysOut pointer+20, byte+596.
    bool push_suppressed_20_596 = false;
};
S3_DERIVE_EQ(PhysicsGroundOutput, skateboard_motion_4, velocity_projection_36, ground_32, state_28, intents_52,
             is_grabbing_object_72_304, manual_opposition_56_168, push_suppressed_20_596)

/// Complete TU3 output leaf. Every comparison uses the same strict relation as
/// its `fcmpu`/branch pair, so unordered values do not select either true path.
PhysicsGroundOutput fill_physics_output(const PhysicsGroundState& state, GroundOutputFrame frame,
                                        GroundOutputSettings settings);

}  // namespace s3::riding::grounded::state::output
