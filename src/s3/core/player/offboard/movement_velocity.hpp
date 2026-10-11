// Port of skate-core/src/player/offboard/movement_velocity.rs.
// Original Biped velocity/turn producer82D7D9B0, TU3.
// Call after movement intent; velocity/turn limits are per physical update.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/point_graph.hpp"
#include "s3/core/rs.hpp"

namespace s3::player::offboard::movement_velocity {

using Vector = std::array<float, 4>;
using point_graph::PointGraph;

struct Settings {
    /// Biped0x490 <- layout0x2B0 Hash_31309236050A8F09.
    PointGraph<8> slope_speed_scalar{};
    /// Biped0x4E0 <- attribute Hash_CE45C724B30F9134.
    PointGraph<8> slope_mode_speed{};
    /// Biped0x530 <- layout0x120 TurnVsSpeed.
    PointGraph<8> turn_vs_speed{};
    /// Biped0x580 <- layout0x170 TurnDeltaVsSpeed.
    PointGraph<8> turn_delta_vs_speed{};
};

struct Input {
    Vector forward{};            // Biped32
    Vector right{};              // Biped0
    Vector plane_normal{};       // Biped544
    float desired_speed = 0.0f;  // Biped756
    float steering = 0.0f;       // Biped760
    bool slope_mode = false;     // Biped714
    bool obstacle = false;       // Biped708
    Vector obstacle_normal{};    // Biped400
    float override_gate = 0.0f;      // input296
    float override_duration = 0.0f;  // input300
    Vector override_velocity{};      // input240
    std::uint32_t flags = 0;         // input176
};

/// Actual Biped fields, decimal offsets. Reset82D7B1C0 sets timer=-1, others0.
struct State {
    Vector velocity{};                 // 480
    float turn = 0.0f;                 // 688
    float forward_delta = 0.0f;        // 692
    float right_delta = 0.0f;          // 696
    float speed = 0.0f;                // 704
    float override_remaining = -1.0f;  // 772

    void update(const Settings& settings, const Input& input);
};
S3_DERIVE_EQ(State, velocity, turn, forward_delta, right_delta, speed, override_remaining)

}  // namespace s3::player::offboard::movement_velocity
