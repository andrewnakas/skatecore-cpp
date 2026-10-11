// Port of skate-core/src/player/wipeout_state/profiles.rs (whole file).
// The five authored physics_wipeout_control profiles and original82D3C3F8.
#pragma once
#include <array>

#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/player/wipeout_state/data.hpp"
#include "s3/core/player/wipeout_state/math.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::player::wipeout_state::profiles {

using math::V;

struct Profile {
    V roll_axis{};                                  // 0
    V horizontal_axis{};                            // 16
    V align_euler{};                                // 32
    point_graph::PointGraph<8> spin_vs_time{};      // 48, source uses X64/Y96
    float direction_follow = 0.0f;                  // 128
    float torque_velocity = 0.0f;                   // 132
    float torque_distance = 0.0f;                   // 136
    float spin_inertia = 0.0f;                      // 140
    bool roll_on_ground = false;                    // 144
    float ground_roll_torque = 0.0f;                // 148
    float sideways_spin = 0.0f;                     // 152
    float forward_spin = 0.0f;                      // 156
    bool horizontal_directed = false;               // 160
    float drift_maximum_speed = 0.0f;               // 164
    float drift_forward = 0.0f;                     // 168
    float drift_sideways = 0.0f;                    // 172
    bool align_with_velocity = false;               // 176
    bool align_ground_with_velocity = false;        // 177
    float ground_align_torque = 0.0f;               // 180
    float tilt_degrees = 0.0f;                      // 184
    float drift_drag = 0.0f;                        // 188
};

void prepare(State& state, const std::array<Profile, 5>& profiles, std::array<float, 2> gesture,
             std::array<float, 2> input);
///Original82D3C928, after air rotational control; preserves body velocity W.
void drift(const State& state, physics::skeleton_body::SkeletonBody& body, const Profile& p);

}  // namespace s3::player::wipeout_state::profiles
