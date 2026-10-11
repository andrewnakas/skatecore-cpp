// Port of skate-game/src/physics/wipeout_states/prediction.rs
//! Original Wipeout Update82D3C02C..C288; completed query precedes next submit.
#pragma once
#include <array>
#include <optional>
#include <string>
#include <tuple>

#include "s3/core/air/trajectory.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/player/wipeout_state.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::wipeout_states::prediction {

struct Prediction {
    ::s3::air::trajectory::QueryResult result = ::s3::air::trajectory::QueryResult::miss();
    std::optional<std::tuple<::s3::air::trajectory::QueryResult, ::s3::air::trajectory::Trajectory, bool>> pending{};

    static Prediction new_() { return Prediction{}; }
    ///Enter clears completion816; the old results are not consumed afterward.
    void reset();
    Status advance(::s3::player::wipeout_state::State& state, const ::s3::physics::board_world::BoardWorld& world,
                   std::array<float, 4> com_position, std::array<float, 4> gravity);
};

}  // namespace s3::game::physics::wipeout_states::prediction
