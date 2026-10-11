// Port of skate-core/src/camera/subject.rs.
//! Normal camera subject accessors and reference points, TU3 82DF69C0/82DF78A8.
//! Physical and animation producers supply this snapshot together, after their
//! tick. No grounded state, bone position, or camera anchor is inferred here.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/camera/_types.hpp"
#include "s3/core/camera/rig.hpp"

namespace s3::camera {

/// Implements the five rig subject interfaces (Rust `impl ... for Subject`) via RigSubject.
struct Subject : RigSubject {
    /// 82DF80D8 -> setter24. This is the native camera subject basis.
    F4x4 transform{};
    /// Physical output bundle5+432 -> setter32.
    F4x4 skeleton_root{};
    /// Physical output bundle5+64 -> setter92, getter444.
    F4 hips_position{};
    /// Physical output bundle8+96 -> setter100, getter452.
    F4 last_valid_ground_up{};
    std::array<F4, 10> reference_positions{};
    std::uint32_t context = 0;
    /// Physical output bundle8+268 -> setter124, getter476.
    float pumping_acceleration = 0.0f;
    /// Physical output bundle7+32 -> setter172, getter524.
    float state_height_32 = 0.0f;
    std::uint8_t in_ground_physics = 0;
    std::uint8_t grinding = 0;
    std::uint8_t trajectory_valid = 0;
    std::uint8_t wiping_out = 0;
    std::uint8_t physically_pushing = 0;
    std::uint8_t at_pushable_speed = 0;
    std::uint8_t off_board = 0;
    /// Bundle2 byte452 -> setter304, getter656. Its producer owns the meaning.
    std::uint8_t air_flag_452 = 0;
    /// Bundle7 byte81 -> setter308, getter660.
    std::uint8_t state_flag_81 = 0;
    /// Bundle15 float200 > 0 and flags204 bit2 -> setter316, getter668.
    std::uint8_t broken_bone_slowmo = 0;
    /// Setter328 is cleared by82DF69C0; later subject writers remain explicit.
    std::uint8_t subject_flag_328 = 0;

    // impl RigModeSubject for Subject
    std::uint8_t flag_568() override { return in_ground_physics; }
    std::uint8_t flag_596() override { return grinding; }
    std::uint8_t flag_680() override { return subject_flag_328; }
    std::uint8_t flag_540() override { return trajectory_valid; }
    // impl AvoidanceSubject for Subject
    F4x4 transform_376() override { return transform; }
    F4 vector_452() override { return last_valid_ground_up; }
    F4 vector_444() override { return hips_position; }
    std::uint32_t context_532() override { return context; }  // also RigPositioningSubject
    std::uint8_t flag_608() override { return off_board; }     // also ReferenceHeightSubject
    std::uint8_t flag_548() override { return wiping_out; }    // also AnchorTracking/RigPositioning
    // impl AnchorTrackingSubject for Subject
    std::uint8_t flag_584() override { return physically_pushing; }
    std::uint8_t flag_588() override { return at_pushable_speed; }
    float value_476() override { return pumping_acceleration; }
    // impl ReferenceHeightSubject for Subject
    std::uint8_t flag_656() override { return air_flag_452; }
    // impl RigPositioningSubject for Subject
    std::uint8_t flag_668() override { return broken_bone_slowmo; }
    std::uint8_t flag_660() override { return state_flag_81; }
    float value_524() override { return state_height_32; }
    F4x4 transform_384() override { return skeleton_root; }
    F4 position_360(std::uint32_t index) override { return reference_positions[index]; }
};
S3_DERIVE_EQ(Subject, transform, skeleton_root, hips_position, last_valid_ground_up, reference_positions, context,
             pumping_acceleration, state_height_32, in_ground_physics, grinding, trajectory_valid, wiping_out,
             physically_pushing, at_pushable_speed, off_board, air_flag_452, state_flag_81, broken_bone_slowmo,
             subject_flag_328)

/// Exact inputs used by82DF78A8; names identify their role in the camera's
/// reference-point table, while comments retain the physical output binding.
struct ReferencePointInputs {
    F4 head{};                   // skeleton+32
    F4 hips{};                   // skeleton+64
    F4 left_foot{};              // skeleton+144
    F4 right_foot{};             // skeleton+160
    F4 board{};                  // skeleton+480
    F4 centre_of_mass{};         // bundle9+64
    F4 damped_centre_of_mass{};  // subject464 (bundle9+80)
    F4 grind_position{};         // bundle4+96
    F4 tracked_anchor{};         // CameraMan604 -> Rig184 -> tracker16
    F4 incline_normal{};         // CameraMan784
    std::uint8_t grinding = 0;

    /// The stock list has ten entries. The collision positioner requests
    /// entries3,0,7, so substituting one board point for this list changes it.
    std::array<F4, 10> positions() const {
        const F4 feet = rs::from_fn<4>([&](std::size_t i) { return (left_foot[i] + right_foot[i]) * 0.5f; });
        return {
            head, hips, grinding != 0 ? grind_position : board, centre_of_mass, board, centre_of_mass,
            tracked_anchor, feet, damped_centre_of_mass, incline_normal,
        };
    }
};
S3_DERIVE_EQ(ReferencePointInputs, head, hips, left_foot, right_foot, board, centre_of_mass, damped_centre_of_mass,
             grind_position, tracked_anchor, incline_normal, grinding)

}  // namespace s3::camera
