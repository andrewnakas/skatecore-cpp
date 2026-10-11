// Port of skate-game/src/graph_host/motion_gameplay_conditions.rs (out-of-line items).
#include "s3/game/graph_host/motion_gameplay_conditions.hpp"

#include "s3/data/result.hpp"

namespace s3::game::graph_host::motion_gameplay_conditions {

namespace {
/// Rust `{:?}` of `Option<&str>`.
std::string debug_option(const std::string* value) {
    return value ? "Some(" + s3::data::debug_str(*value) + ")" : std::string("None");
}
}  // namespace

bool GameplayCondition::recognizes(std::string_view name) {
    return name == "IsRetrievingSkateboard" || name == "IsDroppingSkateboard" || name == "IsInBipedAir" ||
           name == "IsHippyHurdling" || name == "PhysicsWantsRunout" || name == "IsPhysicsWiping" ||
           name == "IsBodyFlipping" || name == "PhysicsWantsWipeOut" || name == "IsBumped" ||
           name == "CanEnterSlide" || name == "IsGrabbingObject" || name == "IsDark" ||
           name == "IsUnderflipRequested" || name == "IsDarkCatchRequested" || name == "OkToDoTrickOnStairs" ||
           name == "IsHandPlanting" || name == "IsFootPlanting" || name == "ShouldPrepareOneFootAirForFootplant" ||
           name == "HasNewHandPlantPos" || name == "ShouldPlayHandPlantAnim" || name == "IsEnteringSkitch" ||
           name == "IsSkitching" || name == "IsMovingObject";
}

rs::Result<GameplayCondition, std::string> GameplayCondition::parse(const Attributes& a) {
    const std::string* name_ptr = a.text("name");
    const std::string name = name_ptr ? *name_ptr : std::string();
    if (name == "IsRetrievingSkateboard") return GameplayCondition(RetrievingBoard{});
    if (name == "IsDroppingSkateboard") return GameplayCondition(DroppingBoard{});
    if (name == "IsInBipedAir") return GameplayCondition(InBipedAir{});
    if (name == "IsHippyHurdling") return GameplayCondition(HippyHurdling{});
    if (name == "PhysicsWantsRunout") return GameplayCondition(WantsRunout{});
    if (name == "IsPhysicsWiping") return GameplayCondition(PhysicsWiping{});
    if (name == "IsBodyFlipping") return GameplayCondition(BodyFlipping{});
    if (name == "PhysicsWantsWipeOut") return GameplayCondition(WantsWipeout{});
    if (name == "IsBumped") return GameplayCondition(Bumped{});
    if (name == "CanEnterSlide") return GameplayCondition(CanEnterSlide{a.boolean_byte("right", 1) != 0});  //82BC53F0
    if (name == "IsGrabbingObject") return GameplayCondition(GrabbingObject{});
    if (name == "IsDark") return GameplayCondition(Dark{});
    if (name == "IsUnderflipRequested") return GameplayCondition(UnderflipRequested{});
    if (name == "IsDarkCatchRequested") return GameplayCondition(DarkCatchRequested{});
    if (name == "OkToDoTrickOnStairs") return GameplayCondition(OkToDoTrickOnStairs{});
    if (name == "IsEnteringSkitch") return GameplayCondition(EnteringSkitch{});
    if (name == "IsSkitching") return GameplayCondition(Skitching{});
    if (name == "IsFootPlanting") return GameplayCondition(FootPlanting{});
    if (name == "ShouldPrepareOneFootAirForFootplant") return GameplayCondition(PrepareFootplant{});
    if (name == "HasNewHandPlantPos") return GameplayCondition(NewHandplantPosition{});
    if (name == "ShouldPlayHandPlantAnim") {
        const std::string* anim = a.text("anim");
        std::size_t phase = 0;
        if (anim && *anim == "antic") {
            phase = 2;
        } else if (anim && *anim == "into") {
            phase = 1;
        } else if (anim && *anim == "out") {
            phase = 0;
        } else {
            return rs::make_err("Unauthored handplant animation phase " + debug_option(anim));
        }
        return GameplayCondition(PlayHandplant{phase});
    }
    if (name == "IsMovingObject") return GameplayCondition(IsMovingObject{});
    if (name == "IsHandPlanting") {
        //82BA54A0 compares these authored strings in this order.
        const std::string* state_ptr = a.text("state");
        const std::string state_text = state_ptr ? *state_ptr : std::string();
        std::uint8_t state = 0;
        if (state_text == "air") {
            state = 0;
        } else if (state_text == "ground") {
            state = 1;
        } else {
            return rs::make_err("IsHandPlanting has unauthored state " + s3::data::debug_str(state_text));
        }
        const std::string* dir_ptr = a.text("dir");
        const std::string dir = dir_ptr ? *dir_ptr : std::string();
        const std::uint8_t direction = dir == "FS" ? 1 : dir == "BS" ? 0 : 2;
        return GameplayCondition(HandPlanting{state, direction});
    }
    return rs::make_err("Unknown gameplay condition " + name);
}

std::optional<bool> GameplayCondition::evaluate_physical(const GameplayConditions& p) const {
    if (is<OkToDoTrickOnStairs>()) return !p.tricks_blocked_on_stairs;
    if (is<RetrievingBoard>()) return p.retrieving_board;  //82BA5EF0:Offboard323
    if (is<InBipedAir>()) return p.in_biped_air;           //82BA8030:Offboard328
    if (is<HippyHurdling>()) return p.hippy_hurdling;      //82BA5DA0:Offboard317
    if (is<WantsRunout>()) return p.wants_runout;          //82BA44B8:State78
    if (is<PhysicsWiping>()) return p.physics_wiping;      //82BA4390:State59
    if (is<BodyFlipping>()) return p.body_flipping;        //82BA71E0:Air441
    if (is<WantsWipeout>()) return p.wants_wipeout;        //82BA4400:State63 || State65
    if (is<Bumped>()) return p.bumped;  //82BA7310: published acceleration and anim_motion/bumps
    if (is<GrabbingObject>()) return p.grabbing_object;  //82BA5700:Offboard304
    if (is<Skitching>()) return p.state == 104;          //82BBBC88:State16
    if (is<IsMovingObject>()) return p.moving_object;
    if (is<EnteringSkitch>()) {
        //82BBBDE0:Ground276,Globals400/layout96
        return !(p.time_to_skitch < 0.0f) && p.time_to_skitch <= p.skitch_transition_time;
    }
    if (const auto* c = std::get_if<HandPlanting>(&value)) {
        //82BA55A8:Air324,State16
        const bool active = c->state == 0 ? p.state == 600 : (p.handplant_flags & 0x80000000u) != 0;
        bool direction = false;
        switch (c->direction) {
            case 2: direction = true; break;
            case 1: direction = (p.handplant_flags & 0x20000000u) != 0; break;
            case 0: direction = (p.handplant_flags & 0x20000000u) == 0; break;
            default: direction = false; break;
        }
        return active && direction;
    }
    // Factory82BCB308 -> VT82321070 slot48=82BBD888.
    // Both graphs read Air448 and Air212, including the entry frame.
    if (is<FootPlanting>()) return p.footplant_active && p.footplant_duration >= 0.0f;
    //82BBD7F0, literals820641A8 and8209975C.
    if (is<PrepareFootplant>()) return p.footplant_contact_time >= 0.1f && p.footplant_contact_time <= 0.5f;
    if (is<NewHandplantPosition>()) return (p.handplant_flags & 0x40000000u) != 0;  //82BBDB98
    if (const auto* c = std::get_if<PlayHandplant>(&value)) {
        //82BBDA98 subtracts one simulation tick before the comparison.
        const float threshold = p.handplant_thresholds.at(c->phase);
        return p.handplant_time - rs::from_bits(0x3c888889u) < (c->phase == 0 ? -threshold : threshold);
    }
    // DroppingBoard | Dark | UnderflipRequested | DarkCatchRequested | CanEnterSlide
    return std::nullopt;
}

}  // namespace s3::game::graph_host::motion_gameplay_conditions
