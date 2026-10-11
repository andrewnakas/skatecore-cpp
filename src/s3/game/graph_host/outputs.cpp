// Port of skate-game/src/graph_host/outputs.rs (out-of-line items).
#include "s3/game/graph_host/outputs.hpp"

#include "s3/data/result.hpp"

namespace s3::game::graph_host::outputs {

namespace {
/// Rust `{:?}` of a `Vec<String>`.
std::string debug_strings(const std::vector<std::string>& values) {
    std::string out = "[";
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out += ", ";
        out += s3::data::debug_str(values[i]);
    }
    return out + "]";
}
}  // namespace

rs::Result<rs::Unit, std::string> GraphCapabilityReport::require_complete(std::string_view graph) const {
    if (unsupported_operations.empty() && unsupported_conditions.empty() && unsupported_hooks.empty()) {
        return rs::Unit{};
    }
    return rs::make_err(std::string(graph) + " contains unsupported authored features: operations=" +
                        debug_strings(unsupported_operations) +
                        ", conditions=" + debug_strings(unsupported_conditions) +
                        ", hooks=" + debug_strings(unsupported_hooks));
}

ActionGraphOutput ActionGraphOutput::from_host(std::uint64_t tick, const IntentMap& action_intents,
                                               const IntentMap& motion_intents,
                                               s3::animation::Slice<const AnimationAttribute> animation_attributes) {
    auto value_or_zero = [](const IntentMap& map, std::string_view name) {
        const float* value = map.get(name);
        return value ? *value : 0.0f;
    };
    ActionGraphOutput out;
    out.tick = tick;
    out.controls.wipeout.request = action_intents.contains_key("WipeOutRequest");
    out.controls.wipeout.air_body_tweak = {value_or_zero(action_intents, "OB_AirBodyTweakX"),
                                           value_or_zero(action_intents, "OB_AirBodyTweakY")};
    const float* gesture_x = action_intents.get("WipeoutGestureX");
    const float* gesture_y = action_intents.get("WipeoutGestureY");
    if (gesture_x && gesture_y) out.controls.wipeout.gesture = std::array<float, 2>{*gesture_x, *gesture_y};
    out.controls.board.drop_requested = action_intents.contains_key("OB_DropBoard");
    out.controls.board.throw_requested = action_intents.contains_key("OB_ThrowBoard");
    out.controls.board.retrieve_requested = action_intents.contains_key("OB_RetrieveBoard");
    out.controls.authored_values_ = AuthoredSignals::from_values(action_intents);
    out.motion_effects = MotionEffects::from_values(motion_intents);
    out.turning = TurningOutput{value_or_zero(motion_intents, "Turn"), value_or_zero(motion_intents, "RawTurn"),
                                value_or_zero(motion_intents, "HardTurn")};
    out.animation_attributes = animation_attributes.to_vec();
    out.effects = GraphEffects{};
    return out;
}

}  // namespace s3::game::graph_host::outputs
