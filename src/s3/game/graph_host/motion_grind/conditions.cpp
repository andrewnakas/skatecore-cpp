// Port of skate-game/src/graph_host/motion_grind/conditions.rs (module `motion_grind::conditions`).
#include "s3/game/graph_host/motion_grind/conditions.hpp"

#include "s3/core/animation/skeleton_input/name.hpp"

namespace s3::game::graph_host::motion_grind::conditions {

using s3::animation::skeleton_input::name::encode;

rs::Result<std::optional<Condition>, std::string> Condition::parse(
    const s3::data::state_graph_attributes::Attributes& a) {
    const std::string* name = a.text("name");
    const std::string n = name ? *name : std::string();
    if (name == nullptr) return std::optional<Condition>();
    if (n == "IsGrindBluntingBackslash") return std::optional<Condition>(BluntingBackslash{});
    if (n == "IsGrindApproach") {
        const std::string* facing = a.text("Facing");
        return std::optional<Condition>(Approach{encode(facing ? *facing : std::string("F")) == encode("F")});
    }
    if (n == "GrindTrickOutTypeAllowed") {
        const std::string* type = a.text("type");
        std::uint32_t value = 0;
        if (type != nullptr && *type == "normal") {
            value = 0;
        } else if (type != nullptr && *type == "left") {
            value = 1;
        } else if (type != nullptr && *type == "right") {
            value = 2;
        } else {
            const std::string shown = type ? "Some(" + s3::data::debug_str(*type) + ")" : std::string("None");
            return rs::make_err("Invalid GrindTrickOutTypeAllowed type: " + shown);
        }
        return std::optional<Condition>(TrickOutType{value});
    }
    if (n == "IsLandingIntoGrind") return std::optional<Condition>(LandingIntoGrind{});
    if (n == "IsDroppingIn") return std::optional<Condition>(DroppingIn{});
    return std::optional<Condition>();
}

bool Condition::evaluate(const Physical& p) const {
    if (std::holds_alternative<BluntingBackslash>(value)) return p.blunting_136 == 4;  //82BBDC08
    if (const auto* c = std::get_if<Approach>(&value)) {
        return p.filtered_grinding_80 && p.approach_268 == static_cast<std::uint32_t>(c->forwards);  //82BBDC80
    }
    if (const auto* c = std::get_if<TrickOutType>(&value)) return p.trick_out_240 == c->value;  //82BBDA10
    if (std::holds_alternative<LandingIntoGrind>(value)) return p.air_grind_443 && p.air_time_184 < 0.2f;  //82BBDD70
    return p.dropping_in_324;  //82BA41F0
}

}  // namespace s3::game::graph_host::motion_grind::conditions
