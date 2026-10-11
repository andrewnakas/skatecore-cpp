// Port of skate-core/src/riding/ground_contact_response.rs.
// Wall-ride contact response, TU3 82D93DF0. Inputs are the native published
// board contact record and processed physical observations, not ray guesses.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/point_graph.hpp"
#include "s3/core/riding/grounded/state/board_types.hpp"

namespace s3::riding::ground_contact_response {

using grounded::state::board_types::GroundContactFrame;
using grounded::state::board_types::GroundContactResponse;
using point_graph::PointGraph;

struct WallRideSettings {
    PointGraph<8> anti_gravity_vs_time{};
    float max_dot_floor_wall = 0.0f;
    float foot_force_time = 0.0f;
    float auto_jump_height = 0.0f;
    float max_time = 0.0f;
    float velocity_time_to_consider = 0.0f;
    float auto_jump_y_down_scalar = 0.0f;
    float auto_jump_force = 0.0f;
};

struct WallRidePhysical {
    /// Processed464,544 and400 respectively.
    std::array<float, 4> board_normal{};
    std::array<float, 4> up{};
    std::array<float, 4> velocity{};
    /// Processed2660,2648,2652,2556.
    float board_mass = 0.0f;
    float gravity = 0.0f;
    float speed = 0.0f;
    std::int32_t contact_count = 0;
};

GroundContactResponse wall_ride_response(const WallRideSettings& settings, WallRidePhysical physical,
                                         GroundContactFrame frame, std::array<float, 4> previous_velocity);

}  // namespace s3::riding::ground_contact_response
