// Port of skate-core/src/player/logic.rs (whole file).
// TU3 PhysicalPlayer Logic phase (`0x82DB5FA8`).
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/player/lifecycle.hpp"
#include "s3/core/player/selector.hpp"
#include "s3/core/player/state.hpp"

namespace s3::player::logic {

using lifecycle::PhysicalPlayerStateLifecycle;
using lifecycle::PhysicalStateCalls;
using lifecycle::SkateboardControllerActions;
using lifecycle::StateBinding;
using lifecycle::StateChangeData;
using selector::StateSelectionInput;
using selector::StateSelector;
using state::UnknownPhysicalState;

/// Player+1876 staging data cleared at the end of every Logic phase.
struct PhysicsOutputStaging {
    /// Bytes +20 through +53.
    std::array<std::uint8_t, 34> bytes_20_through_53{};
    /// Floats +56 through +188.
    std::array<float, 34> scalars_56_through_188{};
    /// Word +200.
    std::uint32_t word_200 = 0;

    void clear_like_82db6000() {
        word_200 = 0;
        bytes_20_through_53.fill(0);
        scalars_56_through_188.fill(0.0f);
    }
};
S3_DERIVE_EQ(PhysicsOutputStaging, bytes_20_through_53, scalars_56_through_188, word_200)

struct LogicResult {
    StateBinding previous;
    StateBinding selected;
    bool changed = false;
};
S3_DERIVE_EQ(LogicResult, previous, selected, changed)

/// State owned by the exact transition portion of PhysicalPlayer's Logic phase.
struct PhysicalPlayerLogic {
    StateSelector selector;
    PhysicalPlayerStateLifecycle lifecycle;
    /// PhysicalPlayer+1304; native writes 100 only when the concrete state changes.
    std::uint32_t frames_since_jump_1304 = 0;

    static PhysicalPlayerLogic new_(state::PhysicalStateId initial_state) {
        return PhysicalPlayerLogic{StateSelector{}, PhysicalPlayerStateLifecycle::new_(initial_state), 0};
    }

    /// Calls current-state GetType, calculates the suggested state, performs
    /// SetPhysicsState only on an edge, then clears the output staging block.
    rs::Result<LogicResult, UnknownPhysicalState> run(const StateSelectionInput& selection_input,
                                                      StateChangeData& state_change_data,
                                                      PhysicsOutputStaging& output_staging, PhysicalStateCalls& states,
                                                      SkateboardControllerActions& controller_actions) {
        const StateBinding previous = lifecycle.active();
        const auto previous_type = states.get_type(previous);
        const auto suggested = selector.calculate(previous_type, selection_input);
        const bool changed = suggested != previous_type;
        StateBinding selected = previous;
        if (changed) {
            frames_since_jump_1304 = 100;
            auto r = lifecycle.set_physics_state(static_cast<std::uint32_t>(suggested), state_change_data, states,
                                                 controller_actions);
            if (r.is_err()) return rs::make_err(r.unwrap_err());
            selected = r.unwrap();
        }
        output_staging.clear_like_82db6000();
        return LogicResult{previous, selected, changed};
    }
};
S3_DERIVE_EQ(PhysicalPlayerLogic, selector, lifecycle, frames_since_jump_1304)

}  // namespace s3::player::logic
