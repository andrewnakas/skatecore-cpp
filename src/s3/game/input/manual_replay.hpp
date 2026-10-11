// Port of skate-game/src/input/manual_replay.rs (`#[cfg(test)]` module in Rust).
//! Opt-in verification device packets, never a gameplay state/force override.
//! The same packets run in production-frame tests and the normal rendered game.
#pragma once
#include <cstdint>

#include "s3/core/input/xbox.hpp"

namespace s3::game::input::manual_replay {

inline constexpr std::uint64_t END_TICK = 330;

inline ::s3::input::xbox::XboxState sample(std::uint64_t tick, std::int16_t sign) {
    ::s3::input::xbox::XboxState state;
    state.buttons = (tick >= 12 && tick < 40) ? std::uint16_t(0x1000) : std::uint16_t(0);
    state.triggers = {0, 0};
    state.left = {0, 0};
    if (tick >= 45 && tick < 195) {
        // Rust `sign * 22000` (i16 multiply; overflow would panic in debug).
        state.right = {0, std::int16_t(sign * 22000)};
    } else {
        state.right = {0, 0};
    }
    return state;
}

}  // namespace s3::game::input::manual_replay
