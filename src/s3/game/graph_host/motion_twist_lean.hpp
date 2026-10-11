// Port of skate-game/src/graph_host/motion_offboard/twist_lean.rs (module `motion_twist_lean`).
//! MatchTwistAndLean stock configuration and existing animation queue adapter.
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "s3/core/animation/playback_parameters.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/core/player/offboard/twist_lean.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/state_graph.hpp"

namespace s3::game::graph_host::motion_twist_lean {

using s3::data::state_graph_attributes::Attributes;
using s3::player::offboard::twist_lean::Observation;
using s3::player::offboard::twist_lean::State;

struct Operation {
    bool always = false;

    ///82BCC490 compares exact bytes through strcmp82AE89B0.
    static Operation parse(const Attributes& attributes) {
        const std::string* update = attributes.text("update");
        return Operation{update != nullptr && *update == "always"};
    }

    ///Supply Some only for available original physical/animation components.
    ///A missing implementation of a physical field must be an upstream error,
    ///not an absent-component value. End is the original82B61BB8 no-op.
    /// `animation`: nullptr = None; any type with `set_attribute(SettableAttribute)`.
    template <class Sink>
    void execute(State& state, std::optional<Observation> observation, Sink* animation, std::uint8_t phase) const {
        using s3::animation::playback_parameters::SettableAttribute;
        using s3::animation::skeleton_input::name::encode;
        switch (phase) {
            case 0: state.begin(observation); break;
            case 1:
                if (animation != nullptr) {
                    const State values = state.update(always, observation);
                    const std::pair<const char*, float> entries[] = {{"Twist", values.twist}, {"Lean", values.lean}};
                    for (const auto& [name, value] : entries) {
                        animation->set_attribute(SettableAttribute{encode(name), value, false, -1});
                    }
                }
                break;
            default: break;
        }
    }
};
S3_DERIVE_EQ(Operation, always)

}  // namespace s3::game::graph_host::motion_twist_lean
