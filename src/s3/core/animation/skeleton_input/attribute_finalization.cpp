// Port of skate-core/src/animation/skeleton_input/attribute_finalization.rs.
#include "s3/core/animation/skeleton_input/attribute_finalization.hpp"

namespace s3::animation::skeleton_input::attribute_finalization {

namespace {
float clamp_control(float value) {
    const float lower = -1.0f - value >= 0.0f ? -1.0f : value;
    return 1.0f - lower >= 0.0f ? lower : 1.0f;
}
}  // namespace

rs::Result<bool, std::string> Accumulator::dispatch(std::string_view name, const ScalarFn& scalar,
                                                    ScalarAttributeInputs& fields, JumpAttributeState& cached,
                                                    ActionMap* map) {
    if (name == "Revert") {
        revert = true;
    } else if (name == "RevertDir") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        revert_direction = v.unwrap();
        fields.flags2472 |= 0x200000u;
    } else if (name == "MinJump") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        minimum_jump = v.unwrap();
    } else if (name == "TrickHeight") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        trick_height = v.unwrap();
        has_trick_height = true;
    } else if (name == "PrepareJump") {
        fields.flags2468 |= 0x200000u;
        if (map) {
            //82590358/825903C8 call stock cInputMap64 and65.
            const float first = clamp_control(map->value(64));
            const float second = clamp_control(map->value(65));
            cached.prepared_controls = {first, second};
        }
    } else if (name == "JumpHeightOverride") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        cached.height_override = v.unwrap();
        cached.height_override_active = true;
    } else {
        return false;
    }
    return true;
}

void Accumulator::finish(ScalarAttributeInputs& fields, ExtendedAttributes& extra, JumpAttributeState& cached,
                         FinalizationInput input, ActionMap* map) const {
    if (revert) {
        extra.revert_direction = revert_direction;
    }
    if ((fields.flags2468 & 0x400000u) != 0) {
        if (input.select_jump_extremes && trick_height <= input.low_jump_threshold) {
            extra.jump_strength = minimum_jump;
        } else if (input.select_jump_extremes && trick_height >= input.high_jump_threshold) {
            extra.jump_strength = 1.0f;
        } else {
            extra.jump_strength = trick_height;
        }
    }
    if (has_trick_height && input.allow_height_override) {
        if (cached.height_override_active) {
            const float difference = cached.height_override - extra.jump_strength;
            if (difference >= 0.0f) {
                extra.jump_strength = cached.height_override;
            }
        }
    } else {
        cached.height_override = 0.0f;
        cached.height_override_active = false;
    }
    if ((extra.flags2480 & 0x1000u) != 0) {
        extra.jump_strength = trick_height;
    }
    if (input.use_prepared_controls) {
        extra.jump_controls = cached.prepared_controls;
    } else if (map) {
        //The suffix deliberately selects68 through8255E010, not65 used
        //by PrepareJump. Keep the recovered source's distinct routing.
        const float first = clamp_control(map->value(64));
        const float second = clamp_control(map->value(68));
        extra.jump_controls = {first, second};
    }
    if (input.external_impulse_active) {
        fields.flags2468 |= 0x40000u;
    }
    //82BDC650 reads a BYTE at10932, the high byte of this big-endian
    //word. The later publications read the complete word separately.
    extra.flags2480 = (extra.flags2480 & ~8u) | ((input.animation_flags >> 24) & 8u);
    fields.flags2484 = (fields.flags2484 & ~2u) | ((input.animation_flags >> 22) & 2u);
    fields.flags2484 = (fields.flags2484 & 0x7fffffffu) | ((input.animation_flags << 6) & 0x80000000u);
    fields.flags2484 = (fields.flags2484 & ~0x01000000u) | ((input.animation_flags >> 4) & 0x01000000u);
    if ((fields.flags2476 & 0x600u) != 0) {
        fields.flags2476 &= ~0x400000u;
    }
}

}  // namespace s3::animation::skeleton_input::attribute_finalization
