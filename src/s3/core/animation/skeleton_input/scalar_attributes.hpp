// Port of skate-core/src/animation/skeleton_input/scalar_attributes.rs.
// Complete contiguous scalar handlers82BDACF0..82BDB694, plus the two
// independently verified scalar PushContact/PushSpeed flag handlers.
// Input order is the packet's MG-then-tree order; no timing/value threshold
// filters are added. process_attributes::process supplies the complete ordered
// dispatch, kind3 events, remaining scalar handlers and finalization.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/output/setup_physics.hpp"
#include "s3/core/animation/skeleton_input/catalog.hpp"

namespace s3::animation::skeleton_input::scalar_attributes {

using catalog::ScalarAttribute;
using output::attributes::AnimationAttribute;
using output::attributes::AttributeName;
using output::setup_physics::PhysicsPacketBinding;

/// These flag words are shared with other ProcessedPhysIn stages. They must
/// be seeded by the real preceding reset/consumer stages, not zeroed here.
struct ScalarAttributeInputs {
    std::uint32_t flags2468 = 0;
    std::uint32_t flags2472 = 0;
    std::uint32_t flags2476 = 0;
    std::uint32_t flags2484 = 0;
    std::uint32_t flags2488 = 0;
    /// Native+2492: left1,right2,up3,down4. Unknown prior enum values survive.
    std::uint32_t board_adjust = 0;
    /// Native2720,2672,2640,2728,2676 respectively.
    float balance = 0.0f;
    float spin = 0.0f;
    float body_spin = 0.0f;
    float brake = 0.0f;
    float turn = 0.0f;
    /// Native2908/2912.
    float turn_scale = 0.0f;
    float magnitude_scale = 0.0f;
    /// Native2880/2864; scalar handlers replace XYZ independently, preserving W.
    std::array<float, 4> animation_end_com{};
    std::array<float, 4> animation_translation{};
    /// Native2896/2904/2900.
    float animation_time = 0.0f;
    float animation_physics_blend_seconds = 0.0f;
    float cadence_end_percent = 0.0f;
    /// Native2712/2716/2732.
    float raw_turn = 0.0f;
    float hard_turn = 0.0f;
    float slide = 0.0f;

    /// Represented ProcessedPhysIn fields written by Reset82BF9EF0.
    /// This is the reset stage, not an implicit reset in attribute dispatch.
    static ScalarAttributeInputs reset(std::uint32_t previous_flags2468, std::uint32_t previous_flags2488) {
        ScalarAttributeInputs out;
        out.flags2468 = (previous_flags2468 & 8u) | 0x2000u;
        out.flags2472 = 0;
        out.flags2476 = 0;
        out.flags2484 = 0;
        out.flags2488 = previous_flags2488 & 0x1fffffu;
        out.board_adjust = 0;
        out.balance = 0.0f;
        out.spin = 0.0f;
        out.body_spin = 0.0f;
        out.brake = 0.0f;
        out.turn = 0.0f;
        out.turn_scale = 1.0f;
        out.magnitude_scale = 1.0f;
        out.animation_end_com = {0.0f, 0.0f, 0.0f, 0.0f};
        out.animation_translation = {0.0f, 0.0f, 0.0f, 0.0f};
        out.animation_time = 0.0f;
        out.animation_physics_blend_seconds = 0.0f;
        out.cadence_end_percent = -1.0f;
        out.raw_turn = 0.0f;
        out.hard_turn = 0.0f;
        out.slide = 0.0f;
        return out;
    }
};
S3_DERIVE_EQ(ScalarAttributeInputs, flags2468, flags2472, flags2476, flags2484, flags2488, board_adjust,
             balance, spin, body_spin, brake, turn, turn_scale, magnitude_scale, animation_end_com,
             animation_translation, animation_time, animation_physics_blend_seconds, cadence_end_percent,
             raw_turn, hard_turn, slide)

struct AnimationControlOutput {
    /// AnimOut+0..16: selected Slide/Grind name, copied as five exact words.
    AttributeName grind_name;
    std::uint32_t flags = 0;
};
S3_DERIVE_EQ(AnimationControlOutput, grind_name, flags)

/// Rust `enum DispatchError`.
struct DispatchError {
    /// Active kind3 requires node lookup82530D80 even for unknown event names.
    struct EventConsumerUnavailable {
        AttributeName name;
    };
    struct KnownScalarUnavailable {
        ScalarAttribute attribute;
    };
    struct UninitializedScalar {
        ScalarAttribute attribute;
    };
    std::variant<EventConsumerUnavailable, KnownScalarUnavailable, UninitializedScalar> value;
    DispatchError(EventConsumerUnavailable v) : value(v) {}
    DispatchError(KnownScalarUnavailable v) : value(v) {}
    DispatchError(UninitializedScalar v) : value(v) {}
};
S3_DERIVE_EQ(DispatchError::EventConsumerUnavailable, name)
S3_DERIVE_EQ(DispatchError::KnownScalarUnavailable, attribute)
S3_DERIVE_EQ(DispatchError::UninitializedScalar, attribute)
S3_DERIVE_EQ(DispatchError, value)

/// Rust `{:?}` text of a DispatchError (used in process_attributes error messages).
std::string debug_string(const DispatchError& error);

struct PacketDispatchError {
    std::size_t attribute_index = 0;
    DispatchError cause;
};
S3_DERIVE_EQ(PacketDispatchError, attribute_index, cause)

/// Dispatch actual packet attributes into their physical consumer fields.
/// On failure earlier attribute effects remain; the failed handler performs no
/// writes here. Success means these supported handlers ran, not that native
/// attribute finalization or Skeleton::ProcessData has completed.
rs::Result<rs::Unit, PacketDispatchError> dispatch_supported_packet(const PhysicsPacketBinding& binding,
                                                                    ScalarAttributeInputs& input,
                                                                    AnimationControlOutput& output);

rs::Result<rs::Unit, DispatchError> dispatch_attribute(const AnimationAttribute& attribute,
                                                       ScalarAttributeInputs& input,
                                                       AnimationControlOutput& output);

}  // namespace s3::animation::skeleton_input::scalar_attributes
