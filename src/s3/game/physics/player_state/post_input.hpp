// Port of skate-game/src/physics/player_state/post_input.rs.
//! Original PostInput82DB5588 with the live ballistic trajectory selector.
//! The authored world has no grind edges or grab-spline descriptors.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/player/input_phase.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::player_state::post_input {

struct PostInputState {
    std::array<std::uint32_t, 4> jump_reference{};
    std::uint32_t jump_fix_frames = 0;
    std::uint32_t latch_frames = 0;
    std::uint32_t state_frames = 0;
    float heading_adjust = 0.0f;
    bool complete = false;
    bool trajectory_pending = false;
    bool trajectory_valid = false;
    bool trajectory_available = false;
    bool trajectory_new_candidate = false;

    static PostInputState new_();
};

///Scorer/decision state that still advances in the genuine no-candidate path
///82D8AB08 ->82D86808/82D86C88/82D86DE8/82D89F58/82D739D8.
struct EmptyEdgePost {
    std::uint32_t free_frames = 0;
    std::uint32_t duration = 0;
    std::uint32_t engage_state = 0;
    float expiry = 0.0f;
    std::uint32_t pop_frames = 0;
    std::array<float, 2> lean{};
    std::uint32_t reset_frames = 0;
    std::uint32_t previous_grind = 0;
    float blend = 0.0f;

    /// Private in Rust.
    static EmptyEdgePost new_();
    void advance(sc::player::input_phase::ProcessedPhysicsInput& p);
};

/// Private in Rust.
std::uint32_t decrement(std::uint32_t v);

/// `pub(super)`. (The private `Services` adapter lives in post_input.cpp.)
Status advance(GamePhysics& physics, SkaterRuntime& skater);

}  // namespace s3::game::physics::player_state::post_input
