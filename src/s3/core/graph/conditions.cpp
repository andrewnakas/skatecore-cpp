// Port of skate-core/src/graph/conditions.rs.
#include "s3/core/graph/conditions.hpp"

#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::graph::conditions {

bool PushBrakeInputs::disabled() const {
    const float degrees = trigonometry::asin(ground_axis_y) * rs::from_bits(0x42652EE1);
    return 90.0f - degrees > maximum_ground_angle_degrees || skeleton_disables_push_brake;
}

rs::Result<bool, const char*> ActionCondition::evaluate(
    const ConditionInputs& input, const IntentMap& action_intents, std::optional<std::size_t> current,
    animation::Slice<const std::optional<std::size_t>> parents) const {
    using R = rs::Result<bool, const char*>;
    if (const auto* v = std::get_if<HasIntent>(&value)) {
        const float* found = action_intents.get(v->name);
        // HasAGIntent82BA0EA8 distinguishes presence-only from numeric.
        return R(found != nullptr && (v->numeric.comparison == Comparison::None || v->numeric.matches(*found)));
    }
    if (const auto* v = std::get_if<TimeSinceLastInput>(&value)) {
        if (!input.time_since_last_input)
            return rs::make_err("TimeSinceLastInput needs PhysOut miscellaneous+128");
        return R(v->_0.matches(*input.time_since_last_input));
    }
    if (const auto* v = std::get_if<Speed>(&value)) {
        if (!input.speeds) return rs::make_err("PhysicsSpeedCompare needs skateboard output");
        const SpeedInputs speeds = *input.speeds;
        return R(v->numeric.matches(v->along_skate_z ? speeds.forward_speed : speeds.speed));
    }
    if (const auto* v = std::get_if<SpeedAndSlope>(&value)) {
        if (!input.speeds) return rs::make_err("PhysicsSpeedAndSlopeCompare needs skateboard output+192");
        return R(v->_0.matches(input.speeds->speed_and_slope));
    }
    if (const auto* v = std::get_if<FilteredState>(&value)) {
        if (!input.physical_state) return rs::make_err("PhysFilteredState needs filtered physical state");
        return R(input.physical_state->category == v->_0);
    }
    if (const auto* v = std::get_if<Grinding>(&value)) {
        if (!input.physical_state) return rs::make_err("IsGrinding needs filtered physical state");
        const PhysicalStateInputs& state = *input.physical_state;
        //824714D0 compares the five encoded words, including the
        //case/truncation semantics already used by skeleton attributes.
        namespace name = animation::skeleton_input::name;
        return R(state.grinding && (!v->name || name::encode(*v->name) == name::encode(state.grind_name)));
    }
    if (std::holds_alternative<Mirrored>(value)) {
        if (!input.mirrored) return rs::make_err("IsMirrored needs the motion-graph stance owner");
        return R(*input.mirrored);
    }
    if (std::holds_alternative<RidingFakie>(value)) {
        if (!input.riding_fakie) return rs::make_err("IsRidingFakie needs the motion-graph stance owner");
        return R(*input.riding_fakie);
    }
    if (std::holds_alternative<DisablePushBrake>(value)) {
        if (!input.push_brake)
            return rs::make_err("DisablePushBrake needs ground/skeleton outputs and stock angle");
        return R(input.push_brake->disabled());
    }
    const auto& v = std::get<CurrentState>(value);
    //82C13820 includes the current state itself, then its ancestors.
    std::optional<std::size_t> cursor = current;
    bool matches = false;
    if (v.target) {
        while (cursor) {
            const std::size_t state = *cursor;
            if (state == *v.target) {
                matches = true;
                break;
            }
            cursor = parents[state];
        }
    }
    return R(matches);
}

}  // namespace s3::graph::conditions
