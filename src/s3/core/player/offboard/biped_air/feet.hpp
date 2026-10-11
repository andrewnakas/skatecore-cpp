// Port of skate-core/src/player/offboard/biped_air/feet.rs (whole file).
// Native foot placement82D773F8/82D77558/82D77AE8. The persistent storage is
// shared with the original state56 manager; all contacts come from Processed.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/player/_deps/animation_foot_ik_external.hpp"
#include "s3/core/player/offboard/board_possession/manager.hpp"

namespace s3::player::offboard::biped_air::recovered::feet {

using V = std::array<float, 4>;
using Frame = std::array<V, 4>;
using board_possession::manager::State;

struct Line {
    V position{};
    V normal{};
    std::uint32_t surface = 0;
    bool valid = false;
};

struct Input {
    std::array<Line, 2> lines{};
    std::array<std::array<V, 2>, 2> local_foot_pairs{};
    std::array<std::array<V, 2>, 2> world_foot_pairs{};
    Frame root{};
    Frame inverse_root{};
    Frame effective_root{};
    V position{};
    V velocity{};
    std::uint32_t flags_2476 = 0;
    std::uint32_t flags_2480 = 0;
    std::uint32_t flags_2484 = 0;
    std::uint32_t state = 0;
};

struct Target {
    V position{};
    bool world = false;
    float blend = 0.0f;
    std::optional<V> normal;
};

///Caller clears both100 timers and308/312/316 for the Air-specific entry.
///GeneralUpdate consumes these targets only when2484 sign bit is clear.
std::array<Target, 2> update(State& s, const Input& i);

///82BEDB78 normal setter, shared by both feet after placement.
void set_normal(animation::foot_ik::external::ExternalTarget& target, V normal);

}  // namespace s3::player::offboard::biped_air::recovered::feet
