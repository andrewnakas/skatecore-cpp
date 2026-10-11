// Port of skate-core/src/physics/skeleton_landing.rs.
// Complete TU3 Skeleton::UpdateLandingAdjust82BD9028. This owns the four
// landing modes and history; it does not synthesize physical COM observations.
#pragma once
#include <cstdint>

#include "s3/core/physics/skeleton_board_offset.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::physics::skeleton_landing {

using point_graph::PointGraph;
using skeleton_board_offset::SkateboardOffset;

// private const (exposed for the ported tests, which read it via `use super::*`).
inline constexpr float STEP = rs::from_bits(0x3C888889u);

struct LandingSettings {
    PointGraph<4> manual_blend;
    PointGraph<4> grind_blend;
    PointGraph<4> coffin_height;
    float minimum_height = 0.0f;
    float maximum_velocity = 0.0f;
    float manual_damping = 0.0f;
    float manual_spring = 0.0f;
    float ground_minimum_compression_time = 0.0f;
    float ground_damping = 0.0f;
    float ground_spring = 0.0f;
    float grind_animation_target_time = 0.0f;
    float grind_damping = 0.0f;
    float grind_spring = 0.0f;
    float grind_target_delta = 0.0f;
    float desired_com_height = 0.0f;
    float coffin_time = 0.0f;
    float coffin_maximum_velocity = 0.0f;
    float coffin_blend_frames = 0.0f;
    float coffin_base_height = 0.0f;
};

struct LandingInput {
    std::uint32_t filtered_state = 0;
    std::uint32_t flags_2468 = 0;
    std::uint32_t flags_2472 = 0;
    std::uint32_t flags_2476 = 0;
    float balance = 0.0f;
    /// dot(Processed608 physical COM velocity, Processed544 ground up).
    float physical_com_velocity_along_up = 0.0f;
    /// dot(Skeleton10928 physical COM - Skeleton9680 physical board, up).
    float physical_com_height = 0.0f;
    /// Previous animation COM Skeleton10960.y minus current board pose12672.y.
    float animation_com_height = 0.0f;
};

struct LandingAdjustment {
    // Default: Skeleton ctor82BD7A9C..7ABC.
    bool active = false;
    std::uint32_t kind = 0;
    float time = 0.0f;
    float position = 0.0f;
    float velocity = 0.0f;
    float previous_com_velocity = 0.0f;
    float previous_animation_height = rs::f32::MAX;
    std::uint32_t previous_filtered_state = 0;
    float desired_grind_com = 0.0f;

    static LandingAdjustment default_() { return LandingAdjustment{}; }

    void update(LandingInput input, const LandingSettings& settings, SkateboardOffset& offset);
};

}  // namespace s3::physics::skeleton_landing
