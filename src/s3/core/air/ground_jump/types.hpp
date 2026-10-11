// Port of skate-core/src/air/ground_jump/types.rs.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/point_graph.hpp"

namespace s3::air::ground_jump::types {

using point_graph::PointGraph;

struct GroundJump {
    std::array<float, 4> velocity{};  // JumpInfo0, metres/second
    float scalar_16 = 0.0f;           // 82D93618 initializes and retains zero
    bool active = false;              // JumpInfo20
};

struct GroundJumpMode {
    float minimum_height_64 = 0.0f;  // physics_mode Hash_BE3F74F978D777E5, metres
    float minimum_height_68 = 0.0f;  // JumpMinHeight, metres
    float maximum_height = 0.0f;     // JumpMaxHeight72, metres
};

struct GroundJumpSettings {
    PointGraph<16> vertical_response{};       // physics_jump0
    PointGraph<8> y_scalar_vs_normal_y{};     // 128
    PointGraph<8> speed_scalar_vs_angle{};    // 208
    PointGraph<8> minimum_height_vs_speed{};  // 288
    PointGraph<8> maximum_height_vs_speed{};  // 352
    float speed_response_max_speed = 0.0f;    // 420, metres/second
    float minimum_scalar = 0.0f;              // 424
    float maximum_y_bonus = 0.0f;             // 428, metres/second
    float adjust_z_factor = 0.0f;             // 432
    float adjust_x_factor = 0.0f;             // 436
    float absolute_minimum_height = 0.0f;     // 456, metres
    /// physics_jump.HippyJumpMinHeight/MaxHeight. These are separate from
    /// the ordinary jump-mode bounds in the stock data.
    float hippy_minimum_height = 0.0f;
    float hippy_maximum_height = 0.0f;
};

struct GroundJumpInput {
    std::uint32_t flags_2468 = 0;
    std::uint32_t flags_2480 = 0;
    std::uint32_t flags_2484 = 0;
    std::uint32_t flags_2488 = 0;
    std::array<float, 4> effective_forward{};          // Processed160
    std::array<float, 4> forward{};                    // 352
    std::array<float, 4> current_velocity{};           // 400 metres/second
    std::array<float, 4> ground_reference_position{};  // 480 metres
    std::array<float, 4> reference_up{};               // 544
    std::array<float, 4> filtered_ground_normal{};     // 576, BoardToolkit normal192
    std::array<float, 4> animation_com_position{};     // 592 metres
    std::array<float, 4> prepared_velocity{};          // 704 metres/second
    float jump_strength = 0.0f;                        // 2624, animation output
    std::array<float, 2> jump_controls{};              // 2628/2632, animation output
    float gravity_y = 0.0f;                            // 2648 metres/second squared
    float surface_speed = 0.0f;                        // 2656 metres/second
};

}  // namespace s3::air::ground_jump::types
