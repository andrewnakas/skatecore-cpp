// Port of skate-game/src/graph_host/motion_intent_filter.rs (module `motion_intent_filter`).
#include "s3/game/graph_host/motion_intent_filter.hpp"

#include <optional>

#include "s3/core/animation/playback_parameters.hpp"

namespace s3::game::graph_host::motion_intent_filter {

namespace {
std::uint32_t filter(const std::string* value) {
    // 82BA0A48 and original initializers 82F847C0..4850.
    using s3::animation::playback_parameters::intent_key;
    const auto key = intent_key(value ? std::string_view(*value) : std::string_view("none"));
    const char* const names[] = {"negate", "abs", "oneMinus", "clamp", "angleFlip", "angleRot90", "angleRotN90"};
    for (std::uint32_t index = 0; index < 7; ++index) {
        if (intent_key(names[index]) == key) return index + 1;
    }
    return 0;
}
}  // namespace

Operation Operation::parse(const Attributes& a) {
    auto number = [&](const char* name, float default_value) {
        return rs::from_bits(a.float_bits(name, rs::to_bits(default_value)));
    };
    auto optional = [&](const char* name) -> std::optional<float> {
        const auto* value = a.get(name);
        return value ? std::optional<float>(rs::from_bits(value->float_bits)) : std::nullopt;
    };
    auto text = [&](const char* name) { return a.text(name); };
    const float blend = number("blend", 1.0f);
    Operation out;
    out.intent = text("intent") ? *text("intent") : std::string();
    out.filtered_intent = text("filteredIntent") ? *text("filteredIntent") : std::string();
    out.settings.starting_value = number("startingValue", 0.0f);
    out.settings.default_value = number("defaultValue", 0.0f);
    out.settings.scale = number("scale", 1.0f);
    out.settings.filters = {filter(text("filter1") ? text("filter1") : text("filter")), filter(text("filter2")),
                            filter(text("fakieFilter")), filter(text("mirrorFilter"))};
    out.settings.ramp_time = optional("rampTime");
    out.settings.blend_rising = number("blendRising", blend);
    out.settings.blend_falling = number("blendFalling", blend);
    out.settings.blend_out = optional("blendOut");
    out.settings.clamp_velocity = optional("clampVel");
    out.settings.clamp_acceleration = optional("clampAcc");
    return out;
}

void Operation::begin(State& state, IntentMap& output) const {
    output.insert(filtered_intent, state.begin(settings));
}

void Operation::update(State& state, const IntentMap& input, IntentMap& output, float dt,
                       std::pair<bool, bool> stance) const {
    const float* in = input.get(intent);
    const float value = state.update(settings, in ? std::optional<float>(*in) : std::nullopt, dt, stance);
    output.insert(filtered_intent, value);
}

void Operation::end(IntentMap& output) const { output.remove(filtered_intent); }

}  // namespace s3::game::graph_host::motion_intent_filter
