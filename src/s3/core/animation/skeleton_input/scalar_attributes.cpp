// Port of skate-core/src/animation/skeleton_input/scalar_attributes.rs.
#include "s3/core/animation/skeleton_input/scalar_attributes.hpp"

#include <string_view>

namespace s3::animation::skeleton_input::scalar_attributes {

namespace {
std::string debug_name(const AttributeName& name) {
    std::string out = "AttributeName([";
    for (std::size_t i = 0; i < name._0.size(); ++i) {
        if (i) out += ", ";
        out += std::to_string(name._0[i]);
    }
    return out + "])";
}
std::string debug_attribute(const ScalarAttribute& a) {
    return "ScalarAttribute { name: \"" + std::string(a.name) + "\", encoded_name: " +
           debug_name(a.encoded_name) + ", comparison_site: " + std::to_string(a.comparison_site) + " }";
}
}  // namespace

std::string debug_string(const DispatchError& error) {
    if (const auto* e = std::get_if<DispatchError::EventConsumerUnavailable>(&error.value)) {
        return "EventConsumerUnavailable { name: " + debug_name(e->name) + " }";
    }
    if (const auto* e = std::get_if<DispatchError::KnownScalarUnavailable>(&error.value)) {
        return "KnownScalarUnavailable { attribute: " + debug_attribute(e->attribute) + " }";
    }
    const auto& e = std::get<DispatchError::UninitializedScalar>(error.value);
    return "UninitializedScalar { attribute: " + debug_attribute(e.attribute) + " }";
}

rs::Result<rs::Unit, PacketDispatchError> dispatch_supported_packet(const PhysicsPacketBinding& binding,
                                                                    ScalarAttributeInputs& input,
                                                                    AnimationControlOutput& output) {
    const auto entries = binding.packet().attributes.entries();
    for (std::size_t attribute_index = 0; attribute_index < entries.size(); ++attribute_index) {
        auto r = dispatch_attribute(entries[attribute_index], input, output);
        if (r.is_err()) return rs::make_err(PacketDispatchError{attribute_index, r.unwrap_err()});
    }
    return rs::Unit{};
}

rs::Result<rs::Unit, DispatchError> dispatch_attribute(const AnimationAttribute& attribute,
                                                       ScalarAttributeInputs& input,
                                                       AnimationControlOutput& output) {
    if (attribute.kind != 0) {
        if (attribute.kind == 3 && (attribute.status & 12) != 0) {
            return rs::make_err(DispatchError(DispatchError::EventConsumerUnavailable{attribute.name}));
        }
        return rs::Unit{};
    }
    const ScalarAttribute* entry = catalog::lookup(attribute.name);
    if (!entry) return rs::Unit{};
    auto scalar = [&]() -> rs::Result<float, DispatchError> {
        if (!attribute.payload._0[0]) {
            return rs::make_err(DispatchError(DispatchError::UninitializedScalar{*entry}));
        }
        return rs::from_bits(*attribute.payload._0[0]);
    };
    const std::string_view n = entry->name;
    if (n == "SlideBoard" || n == "SlideNose" || n == "SlideTail" || n == "Grind5050" || n == "Grind5_O" || n == "GrindNose") {
        output.grind_name = attribute.name;
    } else if (n == "GrabWorld") {
        input.flags2476 |= (1u << 22);
    } else if (n == "BoardAdjustLeft") {
        input.board_adjust = 1u;
    } else if (n == "BoardAdjustRight") {
        input.board_adjust = 2u;
    } else if (n == "BoardAdjustUp") {
        input.board_adjust = 3u;
    } else if (n == "BoardAdjustDown") {
        input.board_adjust = 4u;
    } else if (n == "GrindFacingForwards") {
        output.flags |= (1u << 31);
    } else if (n == "GrindFacingBackwards") {
        output.flags &= ~((1u << 31));
    } else if (n == "NoInput") {
        output.flags |= (1u << 30);
        input.flags2472 |= (1u << 23);
    } else if (n == "ExitGrind") {
        input.flags2476 |= (1u << 29);
    } else if (n == "NewAutoPump") {
        output.flags |= (1u << 29);
        //82BDBC30..48: scalar value is not tested.
    } else if (n == "PlayerControlledPump") {
        input.flags2476 |= 2u;
    } else if (n == "EnteringCoffin") {
        input.flags2472 |= 2u;
        input.flags2476 |= (1u << 30);
    } else if (n == "InCoffin") {
        input.flags2472 |= 1u;
        input.flags2476 |= (1u << 30);
    } else if (n == "LeavingCoffin") {
        input.flags2476 |= 0xc0000000u;
    } else if (n == "PlayingLanding") {
        input.flags2476 |= (1u << 28);
    } else if (n == "Balance") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        input.balance = -v.unwrap();
    } else if (n == "Spin") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        input.spin = -v.unwrap();
    } else if (n == "KickTurn") {
        output.flags |= (1u << 28);
    } else if (n == "Anticipating") {
        output.flags |= (1u << 27);
    } else if (n == "BodySpin") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        input.body_spin = -v.unwrap();
    } else if (n == "ManualBrake") {
        input.flags2468 |= (1u << 29);
    } else if (n == "Brake") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        input.brake = v.unwrap();
    } else if (n == "Turn") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        input.turn = -v.unwrap();
    } else if (n == "IsStumbling") {
        input.flags2484 |= (1u << 10);
    } else if (n == "IsWipeoutPushOffImpulse") {
        input.flags2484 |= (1u << 8);
    } else if (n == "IsWipeoutPushOff") {
        input.flags2484 |= (1u << 9);
    } else if (n == "IsMuteRFoot") {
        input.flags2484 |= (1u << 7);
    } else if (n == "IsMuteLFoot") {
        input.flags2484 |= (1u << 6);
    } else if (n == "IsForceDownRFoot") {
        input.flags2484 |= (1u << 3);
    } else if (n == "IsForceDownLFoot") {
        input.flags2484 |= (1u << 2);
    } else if (n == "TurnScale") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        input.turn_scale = v.unwrap();
    } else if (n == "MagScale") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        input.magnitude_scale = v.unwrap();
    } else if (n == "AnimEndCOMX") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        input.animation_end_com[0] = v.unwrap();
    } else if (n == "AnimEndCOMY") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        input.animation_end_com[1] = v.unwrap();
    } else if (n == "AnimEndCOMZ") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        input.animation_end_com[2] = v.unwrap();
    } else if (n == "AnimTransX") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        input.animation_translation[0] = v.unwrap();
    } else if (n == "AnimTransY") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        input.animation_translation[1] = v.unwrap();
    } else if (n == "AnimTransZ") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        input.animation_translation[2] = v.unwrap();
    } else if (n == "AnimTime") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        input.animation_time = v.unwrap();
    } else if (n == "AnimPhysBlendSec") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        input.animation_physics_blend_seconds = v.unwrap();
    } else if (n == "IsAnimInterpToEdge") {
        input.flags2488 |= (1u << 27);
    } else if (n == "CadenceEndPercent") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        input.cadence_end_percent = v.unwrap();
    } else if (n == "OB_Traj") {
        input.flags2476 |= (1u << 15);
    } else if (n == "OB_Air") {
        input.flags2476 |= (1u << 7);
    } else if (n == "Carving") {
        input.flags2468 |= (1u << 31);
    } else if (n == "RawTurn") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        input.raw_turn = -v.unwrap();
    } else if (n == "HardTurn") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        input.hard_turn = -v.unwrap();
    } else if (n == "Slide") {
        auto v = scalar();
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        input.slide = -v.unwrap();
    } else if (n == "FrontFlip") {
        input.flags2468 |= (1u << 7);
    } else if (n == "BackFlip") {
        input.flags2468 |= (1u << 6);
    } else if (n == "Grabbing") {
        input.flags2468 |= (1u << 5);
    } else if (n == "WantsLeftAirGrab" || n == "WantsRightAirGrab") {
        input.flags2468 |= (1u << 4);
    } else if (n == "FlipTrick") {
        input.flags2472 |= (1u << 22);
    } else if (n == "WipeOutRecover") {
        input.flags2472 |= (1u << 20);
    } else if (n == "ControlledWipeout") {
        input.flags2472 |= (1u << 19);
        // Independent scalar markers. Neither reads strength or queues force.
    } else if (n == "PushContact") {
        input.flags2488 |= (1u << 29);
    } else if (n == "PushSpeed") {
        input.flags2488 |= (1u << 23);
    } else {
        return rs::make_err(DispatchError(DispatchError::KnownScalarUnavailable{*entry}));
    }
    return rs::Unit{};
}

}  // namespace s3::animation::skeleton_input::scalar_attributes
