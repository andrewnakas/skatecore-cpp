// Port of skate-core/src/air/trajectory/types.rs.
// Launch data82D33448/82BE33D0 and the source-owned caller observations.
#pragma once
#include <cstdint>

#include "s3/core/air/trajectory/math.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::air::trajectory::types {

using math::IDENTITY;
using math::Transform;
using math::Vector;
using math::ZERO;
using point_graph::PointGraph;

/// Default: original ctor82D33448; Fill must supply current physical observations.
struct LaunchInfo {
    Transform reckoning_transform = IDENTITY;  // 0=Reckoning816
    Transform reckoning_inverse = IDENTITY;    // 64=Reckoning944
    Vector start_velocity = ZERO;              // 128=Processed400 m/s
    Vector com_velocity = ZERO;                // 144=Processed704 m/s
    Vector skeleton_vector_160 = ZERO;         // 160=Skeleton16208
    Vector skeleton_vector_176 = ZERO;         // 176=Skeleton16240
    Vector board_position = ZERO;              // 192=Processed112 metres
    Vector animation_com_position = ZERO;      // 208=Processed592 metres
    Vector start_position_override = ZERO;     // 224 metres
    Vector board_position_override = ZERO;     // 240 metres
    float cone_angle_x = 0.0f;                 // 256 degrees
    float cone_angle_z = 0.0f;                 // 260 degrees
    float timestep = 0.0f;                     // 264 seconds
    bool player_jumped = false;                // 268
    bool use_position_override = false;        // 269
    std::uint16_t trajectory_count = 0;        // 270 GetLaunchInfo selects1 or7
};

struct SelectorInput {
    Vector gravity{};                     // PhysicsWorld32 m/s^2
    Vector ground_normal{};               // Processed464
    Vector contact_position{};            // 496 metres
    Vector heading_direction{};           // 512
    Vector reference_up{};                // 544
    float board_vertical_velocity = 0.0f;  // 404 m/s
    float directional_input = 0.0f;        // 2636
    std::uint32_t previous_physics_state = 0;  // 2504
    std::uint32_t flags_2472 = 0;
    std::uint32_t flags_2476 = 0;
    std::uint32_t offboard_flags_1776 = 0;
    float grind_lock_distance = 0.0f;  // active physics_mode80 metres
};

/// Stock skater collection values; offsets refer to physics_trajectory unless stated.
struct SelectorSettings {
    float cone_x = 0.0f;                                  // 576 degrees, Skeleton GetLaunchInfo
    float cone_z = 0.0f;                                  // 568 degrees, Skeleton GetLaunchInfo
    float trajectory_max_time = 0.0f;                     // 428 seconds
    float trajectory_max_drop = 0.0f;                     // 63673FB6DA092B7E metres
    float trajectory_error_start = 0.0f;                  // 432
    float trajectory_error_end = 0.0f;                    // 436
    float speed_factor_min = 0.0f;                        // 440 m/s
    float speed_factor_max = 0.0f;                        // 444 m/s
    PointGraph<8> cone_angle_z_vs_speed{};                // 304
    float cone_x_second_pass = 0.0f;                      // 572 degrees
    float cone_z_second_pass = 0.0f;                      // 564 degrees
    PointGraph<8> landing_time_bonus{};                   // 0 x16/y48
    PointGraph<8> landing_com_scalar_vs_slope{};          // 80 x96/y128
    PointGraph<8> landing_force_scalar{};                 // 240
    PointGraph<8> grind_penalty_vs_distance{};            // 160 x176/y208
    float score_middle_bonus = 0.0f;                      // 468
    float score_landing_force = 0.0f;                     // 472
    float score_landing_direction = 0.0f;                 // 476
    float score_transition = 0.0f;                        // 456
    float surface_unrideable_score = 0.0f;                // 460
    float surface_dont_align_score = 0.0f;                // 464
    float natural_air_off_verts_scalar = 0.0f;            // 484
    float minimum_valid_time = 0.0f;                      // 488 seconds
    float minimum_time_after_apex = 0.0f;                 // 492 seconds
    float minimum_normal_delta_second_pass = 0.0f;        // 496 radians
    float maximum_trajectory_adjust = 0.0f;               // 500
    float wall_ride_test_distance = 0.0f;                 // 416 metres
    float wall_ride_minimum_height = 0.0f;                // 420 metres
    float wall_ride_angle_allow_landing = 0.0f;           // 424
    float wall_ride_height_score = 0.0f;                  // 452
    PointGraph<4> wall_ride_boost{};                      // 368 x384/y400
    float wall_ride_normal_dot_limit = 0.0f;              // physics global188/layout248
    PointGraph<8> displacement_vs_speed{};                // physics_reckoning544
    PointGraph<8> displacement_vs_ground_normal{};        // physics_reckoning608
    float trajectory_radius = 0.0f;                       // physics_reckoning C6710942E297D587 metres
    float trajectory_displacement = 0.0f;                 // physics_reckoning runtime attribute
    std::int32_t minimum_trajectory_frames = 0;           // physics_reckoning runtime attribute
    float vert_jump_align_factor = 0.0f;                  // 0B42DFBB7F7756A0
    float vert_jump_align_max_ground_normal_y = 0.0f;     // 94923D136EA2DA37
    float vert_jump_align_min_direction_y = 0.0f;         // 58BFC8D41216C7EE
    float vert_jump_align_max_angle = 0.0f;               // 723FF102068622CF
};

}  // namespace s3::air::trajectory::types
