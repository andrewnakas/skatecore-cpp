// Port of skate-game/src/graph_host/motion_offboard/cadence.rs (module `motion_offboard_cadence`).
#include "s3/game/graph_host/motion_offboard_cadence.hpp"

#include "s3/core/animation/skeleton_input/name.hpp"

namespace s3::game::graph_host::motion_offboard_cadence {

using s3::animation::playback_parameters::SettableAttribute;
using s3::animation::skeleton_input::name::encode;

std::optional<Operation> parse_operation(const Attributes& a) {
    const std::string* name = a.text("name");
    if (name == nullptr) return std::nullopt;
    if (*name == "BipedCadence") return Operation::BipedCadence;
    if (*name == "MatchCadence") return Operation::MatchCadence;
    return std::nullopt;
}

void biped_cadence(std::optional<float> physical_phase, float* animation_phase, std::uint8_t phase) {
    if (phase == 1) {
        if (physical_phase && animation_phase != nullptr) {
            *animation_phase = *physical_phase;
        }
    }
}

void MatchCadence::begin(std::optional<float> physical_phase, bool animation_present) {
    if (physical_phase && animation_present) {
        pending = true;
        captured_phase = *physical_phase;
    }
}

void MatchCadence::update(AttributeSink* animation) {
    if (animation != nullptr) {
        animation->set_attribute(SettableAttribute{encode("CadenceStartPercent"), captured_phase, false, -1});
        pending = false;
    }
}

rs::Result<LocoState, std::string> LocoState::parse(const Attributes& a) {
    const std::string* value = a.text("locostate");
    std::uint32_t expected = 0;
    if (value != nullptr && *value == "Stand") {
        expected = 0;
    } else if (value != nullptr && *value == "Walk") {
        expected = 1;
    } else if (value != nullptr && *value == "Run") {
        expected = 2;
    } else if (value != nullptr && *value == "Sprint") {
        expected = 3;
    } else {
        const std::string shown = value ? "Some(" + s3::data::debug_str(*value) + ")" : std::string("None");
        return rs::make_err("Invalid stock LocoState locostate " + shown);
    }
    return LocoState{expected};
}

}  // namespace s3::game::graph_host::motion_offboard_cadence
