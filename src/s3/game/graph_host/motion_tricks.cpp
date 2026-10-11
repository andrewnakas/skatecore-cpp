// Port of skate-game/src/graph_host/motion_tricks.rs (out-of-line items).
#include "s3/game/graph_host/motion_tricks.hpp"

namespace s3::game::graph_host::motion_tricks {

using s3::animation::skeleton_input::name::encode;

std::optional<Operation> Operation::parse(const Attributes& a) {
    const std::string* name_ptr = a.text("name");
    if (!name_ptr) return std::nullopt;
    const std::string& name = *name_ptr;
    auto text_or_empty = [&](const char* key) {
        const std::string* v = a.text(key);
        return v ? *v : std::string();
    };
    if (name == "FootPlantAbsorb") return Operation(FootPlantAbsorb{});
    if (name == "SetHandPlantAnticLength") return Operation(HandplantAntic{});
    if (name == "ScoringHandPlants") {
        HandplantScore out;
        out.base = text_or_empty("handplantname");
        const std::array<const char*, 4> directions = {"up", "left", "down", "right"};
        for (std::size_t i = 0; i < 4; ++i) {
            if (const std::string* v = a.text(directions[i])) out.directions[i] = *v;
        }
        out.intents = {text_or_empty("intentX"), text_or_empty("intentY")};
        return Operation(std::move(out));
    }
    if (name == "SetTrickHeight") {
        Height out;
        out.from = encode(text_or_empty("from"));
        out.rename = encode(text_or_empty("rename"));
        if (const auto* v = a.get("setToValue")) out.value = rs::from_bits(v->float_bits);
        out.manual = a.get("trickFromManual") != nullptr;
        out.grind = a.get("trickFromGrind") != nullptr;
        return Operation(out);
    }
    if (name == "SetTrickAttr") return Operation(Attribute{encode(text_or_empty("trick"))});
    if (name == "ScoringTrick") return Operation(Scoring{encode(text_or_empty("trick"))});
    if (name == "MonitorUnderflip") return Operation(MonitorUnderflip{});
    if (name == "SetDark") return Operation(Dark{});
    if (name == "UpdateIsWeightOnNose") return Operation(WeightOnNose{});
    return std::nullopt;
}

}  // namespace s3::game::graph_host::motion_tricks
