// Port of skate-game/src/graph_host/motion_offboard/match_air_time.rs (module `motion::match_air_time`).
//! Native MatchAirTime: factory82BCA308, Begin82BB9F48,
//! Update82BBA070, instance82BBA390. End is the empty82B61BB8.
#pragma once
#include <array>
#include <optional>

#include "s3/core/rs.hpp"
#include "s3/game/graph_host/motion_animation.hpp"

namespace s3::game::graph_host::motion::match_air_time {

using motion_animation::MotionAnimation;

/// Rust-private fields and helpers are public here (its tests read them).
struct State {
    float cadence_start = 0.0f;
    float duration = 0.0f;
    std::array<float, 4> translation{};
    bool first_update = false;

    void begin(float cadence, float duration, std::array<float, 4> translation);
    void update(MotionAnimation& animation, float remaining, float duration, std::array<float, 4> translation);
    /// Private in Rust.
    std::optional<float> seek_fraction(float remaining, float duration) const;
};

/// Private in Rust.
std::array<float, 4> bounded_translation(std::array<float, 4> vector);

}  // namespace s3::game::graph_host::motion::match_air_time
