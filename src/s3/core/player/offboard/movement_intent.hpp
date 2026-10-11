// Port of skate-core/src/player/offboard/movement_intent.rs.
// Biped movement producer82D7CF50, steering merge82D7D5A0 and slide82D7D608.
// Original TU3; state/input offsets are documented at the host boundary.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/point_graph.hpp"
#include "s3/core/rs.hpp"

namespace s3::player::offboard::movement_intent {

using Vector = std::array<float, 4>;
using point_graph::PointGraph;

struct Settings {
    /// Biped320 <- physics_biped350, x+16/y+32.
    PointGraph<4> sprint_speed{};
    /// Biped5D0 <- physics_biped90, x+16/y+80.
    PointGraph<16> normal_speed{};
    /// Biped3F0 <- physics_biped1C0, x+16/y+48; header+8 is cap.
    PointGraph<8> sprint_blend{};
    float sprint_time_cap = 0.0f;
    /// Biped440 <- physics_biped300, x+16/y+48.
    PointGraph<8> slide_steering{};
};

struct Input {
    std::uint32_t flags = 0;       // input176
    bool suppress_minimum = false;  // input318
    float magnitude = 0.0f;         // input308
    float steering = 0.0f;          // input312
    bool sprint_pressed = false;    // input316
    bool edge_active = false;       // input353; also disables sprint
    bool ignore_obstacle = false;   // input432
    Vector direction{};             // input256
    Vector edge_tangent{};          // input400
    Vector edge_point{};            // input416
    Vector right{};                 // Biped0
    Vector up{};                    // Biped16
    Vector forward{};               // Biped32
    Vector position{};              // Biped48
    bool obstacle = false;          // Biped708
    Vector obstacle_normal{};       // Biped400
    bool sliding = false;           // Biped710
    Vector slide_velocity{};        // Biped528
};

/// Retained outputs and timers; native Reset82D7B1C0 initializes these to zero.
struct State {
    float speed = 0.0f;              // 756
    float steering = 0.0f;           // 760
    float sprint_time = 0.0f;        // 764
    float sprint_grace = 0.0f;       // 768
    float secondary_speed = 0.0f;    // 776
    float original_steering = 0.0f;  // 780
    bool obstacle_centered = false;  // 712
    bool edge_aligned = false;       // 713
    Vector edge_target{};            // 672

    /// Once per original physical tick; source uses literal1/60, not graph dt.
    void update(const Settings& settings, const Input& input);
    /// 82D7D5A0: opposite signs replace; same signs retain larger magnitude. (Rust-private.)
    void merge_steering(float correction);
    /// (Rust-private.)
    void apply_slide(const Settings& settings, const Input& input);
};
S3_DERIVE_EQ(State, speed, steering, sprint_time, sprint_grace, secondary_speed, original_steering, obstacle_centered,
             edge_aligned, edge_target)

}  // namespace s3::player::offboard::movement_intent
