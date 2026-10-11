// Port of skate-core/src/player/offboard/air_launch.rs (+ jump.rs, math.rs, packet.rs).
// Original Biped launch82D7BA78. This is separate from onboard LaunchInfo.
// Produces the full packet before Sync replaces position32 with its body point.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <utility>

#include "s3/core/player/offboard/air_launch/math.hpp"
#include "s3/core/player/offboard/air_launch/packet.hpp"
#include "s3/core/player/offboard/controller.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::player::offboard::air_launch {

using Vector = std::array<float, 4>;
using packet::Packet;
using point_graph::PointGraph;

/// Actual physics_biped scalar fields, validated by the stock loader.
struct Settings {
    /// Biped796 <- layout896 (0x380), JumpSpeedScalar.
    float jump_speed_scalar = 0.0f;
    /// Biped792 <- layout900 (0x384), JumpHeight.
    float jump_height = 0.0f;
};

/// Source1120 is a point and1136 is the axis used by mode1's rejection.
struct DepartureGeometry {
    Vector point_1120{};
    Vector axis_1136{};
};
S3_DERIVE_EQ(DepartureGeometry, point_1120, axis_1136)

/// A per-call snapshot from the canonical processed input; no persistent copy.
struct Processed {
    Vector board_position_112{};
    Vector forward_224{};
    Vector up_544{};
    Vector position_592{};
    Vector velocity_608{};
    Vector velocity_912{};
    std::optional<DepartureGeometry> departure_geometry;
    std::uint32_t flags_2472 = 0;
    std::uint32_t flags_2476 = 0;
    std::uint32_t flags_2480 = 0;
    std::uint32_t previous_state_2504 = 0;
    std::uint32_t current_state_2508 = 0;
    std::uint32_t current_category_2512 = 0;
    std::uint32_t previous_category_2516 = 0;
    float raw_x_2692 = 0.0f;
    float raw_z_2688 = 0.0f;
};

///82D7BA90..BB38, including the current/previous selector used by its callers.
std::uint8_t mode(const Processed& p, bool current);

/// Borrow the existing controller state and its movement_velocity.turn_vs_speed.
rs::Result<rs::Unit, const char*> produce(Packet& packet, const controller::State& state,
                                          const PointGraph<8>& turn_vs_speed, Settings settings, const Processed& p,
                                          bool current);

namespace jump {
/// Mode4:82D7C0C0..C7F4 (pub(super)).
std::pair<Vector, Vector> calculate(const controller::State& state, const PointGraph<8>& turn, Settings settings,
                                    const Processed& p, Vector forward);
}  // namespace jump

}  // namespace s3::player::offboard::air_launch
