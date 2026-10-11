// Port of skate-core/src/animation/skeleton_input/attribute_finalization.rs.
// Special scalar handlers and unconditional suffix of82BDA0D0.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "s3/core/animation/compat/input_controller.hpp"
#include "s3/core/animation/skeleton_input/extended_attributes.hpp"
#include "s3/core/animation/skeleton_input/scalar_attributes.hpp"

namespace s3::animation::skeleton_input::attribute_finalization {

using extended_attributes::ExtendedAttributes;
using extended_attributes::ScalarFn;
using input::controller::ActionMap;
using scalar_attributes::ScalarAttributeInputs;

struct JumpAttributeState {
    /// Skeleton16488/16492, cached by PrepareJump.
    std::array<float, 2> prepared_controls{};
    /// Skeleton16496/16511; this cache survives only the native suffix gate.
    float height_override = 0.0f;
    bool height_override_active = false;

    /// Skeleton constructor82BD7A54..82BD7A98, once per skeleton.
    static JumpAttributeState new_() { return JumpAttributeState{}; }
};
S3_DERIVE_EQ(JumpAttributeState, prepared_controls, height_override, height_override_active)

struct FinalizationInput {
    /// Selected global292 layout1872/1876/1880.
    bool select_jump_extremes = false;
    float low_jump_threshold = 0.0f;
    float high_jump_threshold = 0.0f;
    /// Selected state variant layout76.
    bool allow_height_override = false;
    /// Selected global208 layout452.
    bool use_prepared_controls = false;
    /// Actual AnimOutPhysIn10496 and10932.
    bool external_impulse_active = false;
    std::uint32_t animation_flags = 0;
};

/// Function locals are reset on every ProcessAnimAttributes invocation.
struct Accumulator {
    // Private in Rust.
    bool revert = false;
    float revert_direction = 1.0f;
    float minimum_jump = 0.0f;
    float trick_height = 0.0f;
    bool has_trick_height = false;

    static Accumulator new_() { return Accumulator{}; }

    /// `map` is Rust's `&mut Option<&mut dyn ActionMap>` (nullptr = None).
    rs::Result<bool, std::string> dispatch(std::string_view name, const ScalarFn& scalar,
                                           ScalarAttributeInputs& fields, JumpAttributeState& cached,
                                           ActionMap* map);
    /// Consumes the accumulator in Rust.
    void finish(ScalarAttributeInputs& fields, ExtendedAttributes& extra, JumpAttributeState& cached,
                FinalizationInput input, ActionMap* map) const;
};

}  // namespace s3::animation::skeleton_input::attribute_finalization
