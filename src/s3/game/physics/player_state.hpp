// Port of skate-game/src/physics/player_state.rs.
//! PhysicalPlayer startup, PostInput, state selection and state publication.
//! Native control decisions use original TU3 functions and stock collections;
//! host ownership replaces the original player component pointers.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "s3/core/physics/filtered_state.hpp"
#include "s3/core/player/lifecycle.hpp"
#include "s3/core/player/selector.hpp"
#include "s3/core/player/selector/conditions.hpp"
#include "s3/core/player/state.hpp"
#include "s3/core/riding/grounded/state/output.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"
#include "s3/game/physics/player_state/post_input.hpp"
#include "s3/game/physics/player_state/pre_state.hpp"
#include "s3/game/physics/player_state/publication.hpp"
#include "s3/game/physics/player_state/registry.hpp"
#include "s3/game/physics/player_state/selection.hpp"
#include "s3/game/physics/player_state/transition.hpp"
#include "s3/game/physics/player_state/wipeout_output.hpp"

namespace s3::game::physics::player_state {

using sc::player::state::PhysicalStateId;

struct PlayerState {
    registry::StateRegistry registry{};
    sc::player::lifecycle::PhysicalPlayerStateLifecycle lifecycle =
        sc::player::lifecycle::PhysicalPlayerStateLifecycle::new_(PhysicalStateId::Sleeping);
    sc::player::selector::StateSelector selector{};
    PhysicalStateId requested_state = PhysicalStateId::Sleeping;
    sc::physics::filtered_state::FilteredState filtered{};
    std::optional<sc::physics::filtered_state::FilteredStateOutput> filtered_output;
    std::optional<sc::riding::grounded::state::output::PhysicsGroundOutput> ground_output;
    post_input::PostInputState post{};
    std::array<bool, 36> state_flags{};
    std::uint32_t state_count = 0;
    std::uint32_t update_count = 0;
    sc::player::selector::conditions::TwoStageThresholds normal_off_ground{};
    sc::player::selector::conditions::TwoStageThresholds skitching_off_ground{};
    float animated_board_threshold = 0.0f;
    bool initialized = false;

    static rs::Result<PlayerState, std::string> load(const sd::Collections& data, std::string_view mode);
    PhysicalStateId current() const;
    ///ResetSystems82DB92D0 resets the selector and physical-output filter;
    ///the coordinator owns board/skeleton resets and the following Ground entry.
    void reset_for_teleport();
};

///Call once before first input; startup constructs the toolkit from live board/reset inputs.
Status initialize(GamePhysics& physics, SkaterRuntime& skater);
///Runs after ProcessInput, before the chosen state's pre-update/force phase.
Status post_input_and_select(GamePhysics& physics, SkaterRuntime& skater);
/// Completes the mandatory PostInput half of the current ProcessInput pass.
/// Host-owned transitions may need to retain their selected state, but they
/// must still drain the query work produced by ProcessInput this tick.
Status complete_post_input(GamePhysics& physics, SkaterRuntime& skater);
///The reset/board/skeleton publications precede this selected-state FillPhysOut.
Status publish(GamePhysics& physics, SkaterRuntime& skater);
///Reset continuation uses the same native SetPhysicsState path and actual entry.
Status enter_after_teleport(GamePhysics& physics, SkaterRuntime& skater);
///Original82DB6050 prefix; coordinator calls the selected state pre-update next.
Status pre_state(GamePhysics& physics, SkaterRuntime& skater);
/// Custom traversal bypasses native queries; discard old work before re-entry.
Status resume_after_climb(GamePhysics& physics, SkaterRuntime& skater);
/// Vehicle ownership has ended; reset first, then enter the native ragdoll and seed momentum.
rs::Result<bool, std::string> apply_vehicle_ejection(GamePhysics& physics, SkaterRuntime& skater);

}  // namespace s3::game::physics::player_state
