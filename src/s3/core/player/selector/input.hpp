// Port of skate-core/src/player/selector/input.rs (whole file).
#pragma once
#include <cstdint>

#include "s3/core/player/selector/conditions.hpp"
#include "s3/core/player/state.hpp"

namespace s3::player::selector::input {

using conditions::BoardBodyState;
using conditions::SkeletonAnimationState;
using conditions::TwoStageThresholds;
using state::PhysicalStateId;

/// Values read by TU3 `PhysicalPlayerStateChanger::CalcSuggestedState`.
/// Offset-bearing names are retained where the producer's semantic name has
/// not yet been proved in Skate 3.
struct ProcessedStateInput {
    std::uint32_t grind_type_1248 = 0;
    bool grind_candidate_1488 = false;
    std::int32_t field_1776 = 0;
    std::uint32_t flags_2468 = 0;
    std::uint32_t flags_2472 = 0;
    std::uint32_t flags_2476 = 0;
    std::uint32_t flags_2480 = 0;
    std::uint32_t flags_2484 = 0;
    std::uint32_t flags_2488 = 0;
    std::uint32_t category_2512 = 0;
    std::int32_t wheel_contact_count_2556 = 0;
    std::uint32_t field_2572 = 0;
    float state_timer_2664 = 0.0f;
    float field_2732 = 0.0f;
    float field_2744 = 0.0f;
    float trajectory_collision_time_2772 = 0.0f;
    std::uint32_t grind_investigation_flags_1516 = 0;

    // pub(crate) in Rust.
    constexpr bool has_2468(std::uint32_t mask) const { return (flags_2468 & mask) != 0; }
    constexpr bool has_2472(std::uint32_t mask) const { return (flags_2472 & mask) != 0; }
    constexpr bool has_2476(std::uint32_t mask) const { return (flags_2476 & mask) != 0; }
    constexpr bool has_2480(std::uint32_t mask) const { return (flags_2480 & mask) != 0; }
    constexpr bool has_2484(std::uint32_t mask) const { return (flags_2484 & mask) != 0; }
    constexpr bool has_2488(std::uint32_t mask) const { return (flags_2488 & mask) != 0; }
    constexpr PhysicalStateId air_variant_from_2468() const {
        return has_2468(0x400) ? PhysicalStateId::KnownAir : PhysicalStateId::PhysicsAir;
    }
};
S3_DERIVE_EQ(ProcessedStateInput, grind_type_1248, grind_candidate_1488, field_1776, flags_2468, flags_2472,
             flags_2476, flags_2480, flags_2484, flags_2488, category_2512, wheel_contact_count_2556, field_2572,
             state_timer_2664, field_2732, field_2744, trajectory_collision_time_2772, grind_investigation_flags_1516)

struct StateSelectionInput {
    ProcessedStateInput processed{};
    /// Byte at `SkateboardBody + 869`.
    std::uint8_t skateboard_contact_count_869 = 0;
    BoardBodyState board_body{};
    SkeletonAnimationState skeleton{};
    /// Three collection values consumed by TU3 `0x82D8BBB8`.
    TwoStageThresholds skitching_off_ground{};
    /// Three physics-mode values consumed by TU3 `0x82D8BD50`.
    TwoStageThresholds normal_off_ground{};
};
S3_DERIVE_EQ(StateSelectionInput, processed, skateboard_contact_count_869, board_body, skeleton,
             skitching_off_ground, normal_off_ground)

}  // namespace s3::player::selector::input
