// Port of skate-core/src/player/wipeout_state/weights.rs (whole file).
// Wipeout Update82D3BE04..82D3BFE8, after the selected physical control.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/player/wipeout_state/data.hpp"

namespace s3::player::wipeout_state::weights {

struct Settings {
    float remove_target_time = 0.0f;
    float remove_drives_time = 0.0f;
    /// Native352 is a per-update increment, not a per-second velocity.
    float collision_step = 0.0f;
    /// Native348 is also a per-update increment.
    float controlled_step = 0.0f;
};

struct Output {
    float target_weight = 0.0f;
    float start = 0.0f;
    float end = 0.0f;
    float controlled = 0.0f;
    float extra = 0.0f;
};

Output update(State& state, Settings settings, bool contact, bool control_applied, std::uint32_t flags_2472,
              std::array<float, 2> gesture);

}  // namespace s3::player::wipeout_state::weights
