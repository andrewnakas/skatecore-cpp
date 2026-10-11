// Port of skate-game/src/graph_host/motion_conditions.rs (out-of-line items).
#include "s3/game/graph_host/motion_conditions.hpp"

#include "s3/game/graph_host/motion_nodes.hpp"

namespace s3::game::graph_host::motion_conditions {

using s3::animation::skeleton_input::name::encode;

namespace {
/// `str::trim_matches(|c: char| c.is_whitespace() || c == '\0')` (ASCII subset).
std::string trim_name(const std::string& text) {
    auto trimmed = [](char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f' || c == '\0';
    };
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && trimmed(text[begin])) ++begin;
    while (end > begin && trimmed(text[end - 1])) --end;
    return text.substr(begin, end - begin);
}

std::string text_or_empty(const Attributes& a, const char* name) {
    const std::string* v = a.text(name);
    return v ? *v : std::string();
}
}  // namespace

rs::Result<std::optional<MotionCondition>, std::string> MotionCondition::parse(const Attributes& a) {
    using R = rs::Result<std::optional<MotionCondition>, std::string>;
    auto some = [](MotionCondition c) { return R(std::optional<MotionCondition>(std::move(c))); };
    {
        auto condition = motion_grind::conditions::Condition::parse(a);
        if (condition.is_err()) return rs::make_err(condition.unwrap_err());
        if (condition.unwrap()) return some(Grind{*condition.unwrap()});
    }
    if (auto condition = motion_landing::Condition::parse(a)) return some(Landing{*condition});
    {
        auto condition = motion_wipeout::Condition::parse(a);
        if (condition.is_err()) return rs::make_err(condition.unwrap_err());
        if (condition.unwrap()) return some(Wipeout{*condition.unwrap()});
    }
    if (auto condition = motion_push_off::IsPushOffEnabled::parse(a)) return some(PushOff{*condition});
    if (auto condition = motion_dismount::Condition::parse(a)) return some(DisableDismount{*condition});
    auto numeric = [&]() { return condition_nodes::numeric(a); };
    const std::string name = trim_name(text_or_empty(a, "name"));
    if (name == "HasTweak") return some(HasTweak{numeric()});
    if (name == "CurrentGrabType") {
        const std::string* grab = a.text("grab");
        if (!grab) return rs::make_err(std::string("CurrentGrabType requires grab"));
        auto parsed = motion_stock_gameplay::grab_type_parse(*grab);
        if (parsed.is_err()) return rs::make_err(parsed.unwrap_err());
        return some(CurrentGrabType{parsed.unwrap()});
    }
    if (name == "ManualOutTimerIsActive") return some(ManualOutTimerIsActive{});
    if (name == "HasGestureIntent") {
        auto group = s3::game::input::gesture_catalog::group_parse(text_or_empty(a, "group"));
        if (group.is_err()) return rs::make_err(group.unwrap_err());
        return some(Gesture{group.unwrap()});
    }
    //82BA59D8/82BA6AC0: missing intent is false even for NotEqual.
    if (name == "HasIntent" || name == "HasFilteredMotionGraphIntent") {
        const std::string* raw = a.text("name");
        return some(Intent{text_or_empty(a, "intent"), raw != nullptr && *raw == "HasFilteredMotionGraphIntent",
                           numeric()});
    }
    if (name == "HasAnimAttribute") {
        return some(AnimationAttribute{encode(text_or_empty(a, "attribute")),
                                       rs::as_i32(rs::from_bits(a.float_bits("sequenceid", rs::to_bits(-1.0f)))),
                                       numeric()});
    }
    if (name == "ExpireInTime") return some(ExpireInTime{numeric()});  //82BA65A0
    if (name == "WillExpire") {
        //82BA6620/82BA6708
        WillExpire out;
        out.in_time = rs::from_bits(a.float_bits("InTime", 0));
        const std::string* tag = a.text("InTimeTag");
        if (tag && !tag->empty()) out.tag = *tag;
        out.wait_for_transitions = a.boolean_byte("waitForTransitions", 1) != 0;
        return some(std::move(out));
    }
    if (name == "InTimeWindow") {
        //82BC4558/82BA6808
        return some(InTimeWindow{rs::from_bits(a.float_bits("StartTime", 0)),
                                 rs::from_bits(a.float_bits("WindowFrameLength", 0))});
    }
    if (name == "InStateForTime") {
        //82BC3788/82BA4658
        return some(InStateForTime{text_or_empty(a, "state"), std::nullopt, numeric()});
    }
    if (name == "InParentStateForTime") return some(InParentStateForTime{std::nullopt, numeric()});  //82BA45D0
    if (name == "IsProSkater") return some(ProSkater{encode(text_or_empty(a, "skater"))});  //82BC56C0/82BA6A48
    if (name == "BreakOutOfPush") return some(BreakOutOfPush{});
    //Factory82BC54B8 and Activation82BA7130.
    if (name == "ShouldLeaveSlide") return some(ShouldLeaveSlide{a.boolean_byte("right", 1) != 0});
    if (name == "IsRidingGoofy") return some(RidingGoofy{});
    if (name == "IsRidingSwitch") return some(RidingSwitch{});
    if (name == "MongoPushFootToFar") return some(MongoPushFootTooFar{});
    if (name == "LastState") return some(LastState{text_or_empty(a, "state"), std::nullopt});
    if (motion_gameplay_conditions::GameplayCondition::recognizes(name)) {
        auto condition = motion_gameplay_conditions::GameplayCondition::parse(a);
        if (condition.is_err()) return rs::make_err(condition.unwrap_err());
        return some(Gameplay{condition.unwrap()});
    }
    if (motion_riding_conditions::MotionRidingCondition::recognizes(name)) {
        auto condition = motion_riding_conditions::MotionRidingCondition::parse(a);
        if (condition.is_err()) return rs::make_err(condition.unwrap_err());
        return some(Riding{condition.unwrap()});
    }
    if (name == "TimeToLand") return some(TimeToLand{numeric()});
    if (name == "OBTimeToLand") return some(ObTimeToLand{numeric()});
    if (name == "OBTrajTime") return some(ObTrajTime{numeric()});
    if (name == "LocoState") {
        auto condition = motion_offboard_cadence::LocoState::parse(a);
        if (condition.is_err()) return rs::make_err(condition.unwrap_err());
        return some(LocoState{condition.unwrap()});
    }
    if (name == "GroundSlopeType") {
        auto condition = motion_ground_slope::GroundSlopeType::parse(a);
        if (condition.is_err()) return rs::make_err(condition.unwrap_err());
        return some(GroundSlopeType{condition.unwrap()});
    }
    if (name == "IsBipedGroundThin") return some(IsBipedGroundThin{});
    if (name == "IsHoldingSkateboard") return some(IsHoldingSkateboard{});
    if (name == "IsStandingOnMovingObject") return some(IsStandingOnMovingObject{});
    if (name == "IsInDebugAnimationsMode") return some(DebugAnimationsMode{});
    if (motion_stock_conditions::Condition::recognizes(name)) {
        return some(StockGameplay{motion_stock_conditions::Condition::parse(a)});
    }
    auto shared = condition_nodes::parse(a);
    if (shared.is_err()) return rs::make_err(shared.unwrap_err());
    if (!shared.unwrap()) return std::optional<MotionCondition>();
    return some(Shared{*shared.unwrap()});
}

void bind_states(const graph_runtime::LoadedGraph& graph,
                 s3::animation::Slice<motion_nodes::MotionOperation> operations) {
    std::vector<std::optional<std::size_t>> parents(graph.source.elements.size());
    for (std::size_t parent = 0; parent < graph.source.elements.size(); ++parent) {
        for (const std::size_t child : graph.source.elements[parent].children) parents.at(child) = parent;
    }
    for (const std::size_t id : graph.runtime.operations.conditions) {
        auto* operation = std::get_if<motion_nodes::MotionOperation::Condition>(&operations[id].value);
        if (!operation) continue;
        MotionCondition& condition = operation->_0;
        const std::string* name = nullptr;
        std::optional<std::size_t>* target = nullptr;
        if (auto* shared = std::get_if<MotionCondition::Shared>(&condition.value)) {
            auto* current = std::get_if<ActionCondition::CurrentState>(&shared->_0.value);
            if (!current) continue;
            name = &current->name;
            target = &current->target;
        } else if (auto* c = std::get_if<MotionCondition::InStateForTime>(&condition.value)) {
            name = &c->name;
            target = &c->target;
        } else if (auto* c = std::get_if<MotionCondition::LastState>(&condition.value)) {
            name = &c->name;
            target = &c->target;
        } else if (auto* c = std::get_if<MotionCondition::InParentStateForTime>(&condition.value)) {
            target = &c->target;
        } else {
            continue;
        }
        std::optional<std::size_t> element = parents.at(graph.binding.operations.at(id).element);
        while (element) {
            const std::size_t element_id = *element;
            std::optional<std::size_t> state;
            for (std::size_t s = 0; s < graph.binding.states.size(); ++s) {
                if (graph.binding.states[s].element == element_id) {
                    state = s;
                    break;
                }
            }
            if (state) {
                // GetParentState82C11B88 climbs expression nodes to the
                // containing state; it does not select that state's parent.
                *target = name ? graph.binding.find_state(*state, *name, true) : std::optional<std::size_t>(*state);
                break;
            }
            element = parents.at(element_id);
        }
    }
}

}  // namespace s3::game::graph_host::motion_conditions
