// Port of skate-core/src/player/selector/mod.rs.
// Readable port of TU3 `PhysicalPlayerStateChanger::CalcSuggestedState`
// (`0x82D8ADE8`) and its directly-called predicates.
// The private submodules air.rs / ground.rs / offboard.rs are member functions
// implemented in selector/{air,ground,offboard}.cpp.
#pragma once
#include <cstdint>
#include <optional>

#include "s3/core/player/selector/conditions.hpp"
#include "s3/core/player/selector/input.hpp"
#include "s3/core/player/state.hpp"

namespace s3::player::selector {

// pub use input::{ProcessedStateInput, StateSelectionInput};
using input::ProcessedStateInput;
using input::StateSelectionInput;
using state::PhysicalStateId;

/// pub(super) in Rust.
struct FrameFacts {
    bool colliding = false;
    bool wipeout = false;
    bool force_known_air = false;
    bool skateboard_animated = false;
    bool off_ground = false;
    bool off_ground_skitching = false;
    std::optional<PhysicalStateId> grind{};
};

struct StateSelector {
    std::optional<PhysicalStateId> current_state{};
    std::int32_t nonspecific_collision_free_frames = 0;
    std::int32_t nonspecific_collision_frames = 0;
    std::int32_t something_colliding_frames = 0;
    std::int32_t two_wheel_counter = 0;
    std::int32_t three_wheel_counter = 0;
    std::int32_t post_grind_jump_counter = 0;
    std::int32_t air_frames = 0;
    std::int32_t teleport_countdown = 0;
    std::int32_t skitch_exit_countdown = 0;
    bool revert_exited_normally = false;
    bool request_teleport = false;

    /// Executes the complete state decision tree and all state-changer-local
    /// counter/flag writes performed by TU3 `0x82D8ADE8`.
    PhysicalStateId calculate(PhysicalStateId current, const StateSelectionInput& input);

    // Private helpers (mod.rs, ground.rs, air.rs, offboard.rs).
    PhysicalStateId select_nonspecific(PhysicalStateId current, const StateSelectionInput& input,
                                       FrameFacts facts) const;
    PhysicalStateId select_ground_family(PhysicalStateId current, const StateSelectionInput& input,
                                         FrameFacts facts);
    PhysicalStateId select_physics_air(PhysicalStateId current, const StateSelectionInput& input,
                                       FrameFacts facts) const;
    PhysicalStateId select_known_air(PhysicalStateId current, const StateSelectionInput& input,
                                     FrameFacts facts) const;
    PhysicalStateId select_grind(PhysicalStateId current, const StateSelectionInput& input,
                                 FrameFacts facts) const;
    PhysicalStateId select_biped_or_plant(PhysicalStateId current, const StateSelectionInput& input,
                                          FrameFacts facts) const;
};
S3_DERIVE_EQ(StateSelector, current_state, nonspecific_collision_free_frames, nonspecific_collision_frames,
             something_colliding_frames, two_wheel_counter, three_wheel_counter, post_grind_jump_counter, air_frames,
             teleport_countdown, skitch_exit_countdown, revert_exited_normally, request_teleport)

/// Private in Rust (`fn check_for_grind`).
std::optional<PhysicalStateId> check_for_grind(ProcessedStateInput input);

}  // namespace s3::player::selector
