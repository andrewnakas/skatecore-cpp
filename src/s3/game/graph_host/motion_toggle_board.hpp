// Port of skate-game/src/graph_host/motion_offboard/toggle_board.rs (module `motion_toggle_board`).
//! Original ToggleBoard lifecycle adapter, TU3 82BA8FD8/8FE8 and 82B61BB8.
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "s3/core/player/offboard/toggle_board.hpp"
#include "s3/core/rs.hpp"
#include "s3/game/graph_host/motion_animation.hpp"
#include "s3/game/graph_host/outputs.hpp"

namespace s3::game::graph_host::motion_toggle_board {

using motion_animation::MotionAnimation;
using outputs::BoardControls;
using s3::player::offboard::toggle_board::State;

/// Completed native output, supplied by the board-possession/physical owner.
struct Physical {
    bool grabbing_object = false;
    bool holding_board = false;
    /// Completed OffBoard312: free/hidden/returning, not merely !held311.
    bool free_board = false;
    bool retrieval_blocked = false;
    bool retrieval_active = false;
    float yaw_radians = 0.0f;
    float pitch_radians = 0.0f;
};

/// `const CHANNEL` (private in Rust).
inline constexpr std::string_view CHANNEL = "RetrieveBoard";

rs::Result<rs::Unit, std::string> execute(State& state, MotionAnimation& animation, BoardControls actions,
                                          std::optional<Physical> physical, std::optional<bool> mirrored,
                                          std::uint8_t phase);

/// Private in Rust (used by its tests).
void publish(MotionAnimation& animation, std::string_view name);
/// Private in Rust (used by its tests).
void attribute(MotionAnimation& animation, std::string_view name, float value);

}  // namespace s3::game::graph_host::motion_toggle_board
