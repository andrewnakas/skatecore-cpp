// Port of skate-game/src/graph_host/action_conditions.rs.
#include "s3/game/graph_host/action_conditions.hpp"

#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/game/graph_host/action.hpp"
#include "s3/game/graph_host/condition_nodes.hpp"
#include "s3/game/graph_host/motion_dismount.hpp"

namespace s3::game::graph_host::action_conditions {

namespace gesture_catalog = s3::game::input::gesture_catalog;

rs::Result<std::optional<ActionCondition>, std::string> ActionCondition::parse(const Attributes& a) {
    using O = std::optional<ActionCondition>;
    auto text_or = [&](const char* key) {
        const std::string* t = a.text(key);
        return t ? *t : std::string();
    };
    const std::string name = text_or("name");
    if (name == "HasGestureIntent") {
        auto group = gesture_catalog::group_parse(text_or("group"));
        if (group.is_err()) return rs::make_err(group.unwrap_err());
        return O(Gesture{group.unwrap()});
    }
    if (name == "HasAnimAttribute") {
        AnimationAttribute out;
        out.name = s3::animation::skeleton_input::name::encode(text_or("attribute"));
        out.sequence_id = rs::as_i32(rs::from_bits(a.float_bits("sequenceid", rs::to_bits(-1.0f))));
        out.numeric = condition_nodes::numeric(a);
        return O(out);
    }
    if (name == "InParentStateForTime") return O(ParentTime{std::nullopt, condition_nodes::numeric(a)});
    if (name == "IsGrabbingObject" || name == "IsHandPlanting" || name == "IsFootPlanting") {
        auto condition = GameplayCondition::parse(a);
        if (condition.is_err()) return rs::make_err(condition.unwrap_err());
        return O(Physical{condition.unwrap()});
    }
    if (name == "IsDroppingIn") return O(DroppingIn{});
    if (name == "IsTricking") return O(Tricking{});
    if (name == "IsInLocomotion") return O(InLocomotion{});
    if (name == "DisableDismount") return O(DisableDismount{});
    if (name == "TimeToLand") return O(TimeToLand{condition_nodes::numeric(a)});
    return O();
}

rs::Result<bool, std::string> ActionCondition::evaluate(const action::ActionHost& host,
                                                        const s3::graph::controller::Frame& frame) const {
    if (const auto* c = std::get_if<Gesture>(&value)) {
        return gesture_catalog::group_has_intent(
            c->_0, [&](const char* intent) { return host.action_intents.contains_key(intent); });
    }
    if (const auto* c = std::get_if<AnimationAttribute>(&value)) {
        return condition_nodes::animation_attribute(host.animation_attributes, c->name, c->sequence_id, c->numeric);
    }
    if (const auto* c = std::get_if<ParentTime>(&value)) {
        return condition_nodes::state_time(frame, c->target, c->numeric);
    }
    if (const auto* c = std::get_if<Physical>(&value)) {
        if (!host.gameplay_conditions) {
            return rs::make_err(std::string("ActionGraph requires actual physical condition outputs"));
        }
        const std::optional<bool> result = c->_0.evaluate_physical(*host.gameplay_conditions);
        if (!result) return rs::make_err(std::string("Condition is not a physical-only original leaf"));
        return *result;
    }
    //82BA56B8 uses specific MotionGraph virtual100=8258F8D8 bit27.
    if (std::holds_alternative<DroppingIn>(value)) {
        if (!host.dropping_in) return rs::make_err(std::string("IsDroppingIn requires completed grind output"));
        return *host.dropping_in;
    }
    if (std::holds_alternative<Tricking>(value)) {
        if (!host.is_tricking) return rs::make_err(std::string("IsTricking requires actual MotionGraph flag27"));
        return *host.is_tricking;
    }
    //Factory82BC3F68 installs8231ED5C; virtual48=8274CA90 is li r3,0;blr.
    if (std::holds_alternative<InLocomotion>(value)) return false;
    if (std::holds_alternative<DisableDismount>(value)) {
        const auto& push_brake = host.condition_inputs.push_brake;
        auto result = motion_dismount::Condition{}.evaluate(push_brake ? &*push_brake : nullptr);
        if (result.is_err()) return rs::make_err(std::string(result.unwrap_err()));
        return result.unwrap();
    }
    const auto& numeric = std::get<TimeToLand>(value)._0;
    if (!host.gameplay_conditions) {
        return rs::make_err(std::string("ActionGraph TimeToLand requires actual physical condition outputs"));
    }
    const auto& p = *host.gameplay_conditions;
    return p.time_to_land_valid && numeric.matches(p.time_to_land);
}

}  // namespace s3::game::graph_host::action_conditions
