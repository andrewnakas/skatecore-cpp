// Port of skate-core/src/animation/skeleton_input/extended_attributes.rs.
// Remaining scalar fields and flags82BDB6A4..82BDC4B4. Values retain their
// preceding ProcessData/reset state until the corresponding attribute writes.
#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include "s3/core/animation/skeleton_input/scalar_attributes.hpp"

namespace s3::animation::skeleton_input::extended_attributes {

using scalar_attributes::AnimationControlOutput;
using scalar_attributes::ScalarAttributeInputs;

/// `impl Fn() -> Result<f32, String>` scalar payload accessor.
using ScalarFn = std::function<rs::Result<float, std::string>()>;

struct ExtendedAttributes {
    std::uint32_t flags2480 = 0;
    /// Processed2796..2820.
    float grind_translation = 0.0f;
    float grind_stability_nudge = 0.0f;
    float grind_up_down = 0.0f;
    float grind_grab_min_height = 0.0f;
    float physical_body_spin = 0.0f;
    float world_grab_y = 0.0f;
    float world_grab_z = 0.0f;
    /// Processed2680..2692,2704..2708,2784..2792.
    float offboard_turn = 0.0f;
    float offboard_magnitude = 0.0f;
    float biped_world_x = 0.0f;
    float biped_world_z = 0.0f;
    float look_x = 0.0f;
    float look_y = 0.0f;
    float object_move_z = 0.0f;
    float object_move_x = 0.0f;
    float object_move_rotation = 0.0f;
    /// Processed2824/2828,2840/2844,2916/2920,2928/2932.
    std::array<float, 2> wipeout_control{};
    std::array<float, 2> offboard_jump{};
    std::array<float, 2> wipeout_gesture{};
    std::array<float, 2> body_adjust{};
    /// Processed2936/2940/2944; AnimOut24.
    float biped_start_angle = 0.0f;
    float biped_spin_angle = 0.0f;
    float biped_animation_time = 0.0f;
    float footstep_strength = 0.0f;
    /// Processed2624/2628/2632/2744.
    float jump_strength = 0.0f;
    std::array<float, 2> jump_controls{};
    float revert_direction = 0.0f;

    /// Reset82BF9EF0 zeros every represented ProcessedPhysIn field here.
    /// AnimOut24 belongs to another owner and survives this reset.
    static ExtendedAttributes reset(float footstep_strength) {
        ExtendedAttributes out{};
        out.footstep_strength = footstep_strength;
        return out;
    }
};
S3_DERIVE_EQ(ExtendedAttributes, flags2480, grind_translation, grind_stability_nudge, grind_up_down,
             grind_grab_min_height, physical_body_spin, world_grab_y, world_grab_z, offboard_turn,
             offboard_magnitude, biped_world_x, biped_world_z, look_x, look_y, object_move_z, object_move_x,
             object_move_rotation, wipeout_control, offboard_jump, wipeout_gesture, body_adjust,
             biped_start_angle, biped_spin_angle, biped_animation_time, footstep_strength, jump_strength,
             jump_controls, revert_direction)

/// Special jump/revert handlers and finalization are in attribute_finalization.
/// Return false only for a name outside this contiguous scalar region.
rs::Result<bool, std::string> dispatch(std::string_view name, const ScalarFn& scalar,
                                       ScalarAttributeInputs& input, ExtendedAttributes& extra,
                                       AnimationControlOutput& output);

}  // namespace s3::animation::skeleton_input::extended_attributes
