// Port of skate-game/src/crash_context.rs.
//! SIMREF: only `requested` from skate-game/src/crash_context.rs (crash-report ring buffer).
//! It is a pure observer of state transitions; the Bevy sampling system is removed.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <tuple>

#include "s3/core/player/state.hpp"

namespace s3::game::crash_context {

using Transition =
    std::tuple<std::uint64_t, ::s3::player::state::PhysicalStateId, ::s3::player::state::PhysicalStateId>;

struct Events {
    std::array<std::optional<Transition>, 64> ring{};
    std::size_t index = 0;
    std::uint64_t count = 0;
};
inline Events& events() {
    static Events e;
    return e;
}

inline void requested(std::uint64_t tick, ::s3::player::state::PhysicalStateId from,
                      ::s3::player::state::PhysicalStateId to) {
    Events& e = events();
    const std::size_t index = e.index;
    e.ring[index] = Transition{tick, from, to};
    e.index = (index + 1) % 64;
    e.count += 1;
}

}  // namespace s3::game::crash_context
