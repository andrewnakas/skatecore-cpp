// Compat for skate-game/src/input/gesture_mapping.rs (crate::input, not yet ported).
// FALLBACK OWNED BY STAGE A: delete once s3/game/input/gesture_mapping.hpp exists.
//! GestureTrickMapping82BA07F0 and CreateTrickIntentFromGesture82BA1C30..2228.
#pragma once
#if __has_include("s3/game/input/gesture_mapping.hpp")
#include "s3/game/input/gesture_mapping.hpp"
#else
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "s3/core/animation/playback_parameters.hpp"
#include "s3/core/graph/intents.hpp"
#include "s3/game/graph_host/compat/input_gesture_catalog.hpp"
#include "s3/game/graph_host/compat/input_gesture_mapping_data.hpp"

namespace s3::game::input::gesture_mapping {

using gesture_catalog::Group;
using s3::graph::intents::IntentMap;

/// Returns nullptr for None.
inline const char* select(Group group, const IntentMap& intents, bool mirrored) {
    const auto rows = gesture_mapping_data::TABLES[static_cast<std::size_t>(group)];
    // 82BC0D90 uses greatest prime <=46, then maximum load10000.
    // 82BC1BE8 prepends entries; 82BA07F0 visits ascending buckets.
    const gesture_mapping_data::Row* selected = nullptr;
    std::size_t bucket = SIZE_MAX;
    for (std::size_t i = rows.size(); i-- > 0;) {
        const gesture_mapping_data::Row& row = rows[i];
        std::uint32_t sum = 0;
        for (std::uint32_t word : s3::animation::playback_parameters::intent_key(row._0)) sum += word;
        const std::size_t current = static_cast<std::size_t>(sum) % 43;
        if (current < bucket && intents.contains_key(row._0)) {
            selected = &row;
            bucket = current;
        }
    }
    if (selected == nullptr) return nullptr;
    const gesture_mapping_data::Row& row = *selected;
    mirrored = mirrored && !intents.contains_key("DontMirrorTrick");
    if (intents.contains_key("DarkCatch") && row._3 != 0) {
        if ((row._3 == 2 && !mirrored) || (row._3 == 1 && mirrored)) return "Heelflip";
        if ((row._3 == 3 && mirrored) || (row._3 == 4 && !mirrored)) return "N_Heelflip";
        if ((row._3 == 3 && !mirrored) || (row._3 == 4 && mirrored)) return "N_Kickflip";
        return "Kickflip";
    }
    return mirrored ? row._2 : row._1;
}

/// Manual `impl Default`: `State{}` == `State::default_()`.
struct State {
    bool first_update = true;
    std::optional<std::string> selected;
    const char* successor = nullptr;  // Option<&'static str>

    static State default_() { return State{}; }

    void begin(Group group, std::optional<std::string_view> override_name, const IntentMap& ag, IntentMap& mg,
               bool mirrored) {
        const char* selected_name = select(group, ag, mirrored);
        if (selected_name == nullptr) return;
        std::string name = override_name ? std::string(*override_name) : std::string(selected_name);
        if (ag.contains_key("Underflip")) name = "U_" + name;
        if (name == "Kickflip") {
            successor = "KickflipHold";
        } else if (name == "Heelflip") {
            successor = "HeelflipHold";
        } else if (name == "N_Kickflip") {
            successor = "N_KickflipHold";
        } else if (name == "N_Heelflip") {
            successor = "N_HeelflipHold";
        } else {
            successor = nullptr;
        }
        selected = name;
        mg.insert(name, 0.0f);
    }
    void update(IntentMap& mg) {
        if (!first_update) {
            if (selected && successor != nullptr) {
                mg.remove(*selected);
                mg.insert(successor, 0.0f);
            }
        }
        first_update = false;
    }
    void end(IntentMap& mg) {
        mg.remove("DarkCatch");
        if (selected) {
            if (successor != nullptr) mg.remove(successor);
            mg.remove(*selected);
        }
    }
};

}  // namespace s3::game::input::gesture_mapping
#endif
