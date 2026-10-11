// Port of skate-game/src/physics/grind/state.rs.
//! Separate native state instances: common ctor82D3F210 and Enter82D3F318.
#pragma once
#include <array>
#include <cstdint>

#include "s3/game/physics/grind/family.hpp"
#include "s3/game/physics/grind/output.hpp"

namespace s3::game::physics::grind::state {

using V = std::array<float, 4>;

struct State {
    output::State output;
    std::array<V, 4> frame{};
    bool already_jumped = false;
    std::int32_t updates = 0;
    std::int32_t leaving_updates = 0;
    bool preparing_jump = false;

    static State new_();
    void enter(Family family, std::array<V, 4> board);
};

}  // namespace s3::game::physics::grind::state
