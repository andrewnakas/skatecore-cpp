// Port of skate-core/src/camera/compass.rs (+ compass_movement.rs, compass_orbit.rs methods).
//! Nine stock gameplay compass headings, TU382DF3770/82DF39B0. These are
//! subject directions consumed by authored shots, not replay camera controls.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/camera/_types.hpp"

namespace s3::camera {

struct ManagerSubject;

struct CompassSettings {
    float heading_response = 0.0f;        // parameter604
    float time_before_lineup = 0.0f;      // 880
    float lineup_speed = 0.0f;            // 884
    float minimum_deadzone_speed = 0.0f;  // 888
    float maximum_deadzone_speed = 0.0f;  // 892
    float maximum_deadzone_size = 0.0f;   // 896
    float deadzone_smoothing = 0.0f;      // 900
};
S3_DERIVE_EQ(CompassSettings, heading_response, time_before_lineup, lineup_speed, minimum_deadzone_speed,
             maximum_deadzone_speed, maximum_deadzone_size, deadzone_smoothing)

struct CompassInputs {
    F4 ground_normal{};   // 0: subject452
    F4 landing_normal{};  // 16: subject412 or448
    /// Publisher cache80, also published as subject376: skeleton root when
    /// offboard/wiping out, otherwise the previous physical transform.
    F4x4 transform{};             // 32
    F4 trajectory_direction{};    // 96: subject420 or normalized392
    F4 launch_position{};         // 112
    F4 landing_position{};        // 128
    F4 grind_direction{};         // 144: subject424
    F4 skeleton_direction{};      // 160: subject440=Skeleton output0
    F4 camera_position{};         // 176
    F4 look_target{};             // 192: subject468=output6+64
    std::array<float, 2> look{};  // 208
    float trajectory_fraction = 0.0f;  // 224
    std::uint32_t selected_compass = 0;  // 228
    bool no_trajectory = false;  // 232bit80
    bool state_103 = false;      // bit40: subject564
    bool wiping_out = false;     // bit20
    bool grinding = false;       // bit10
    bool air_flag_452 = false;   // bit8
};
S3_DERIVE_EQ(CompassInputs, ground_normal, landing_normal, transform, trajectory_direction, launch_position,
             landing_position, grind_direction, skeleton_direction, camera_position, look_target, look,
             trajectory_fraction, selected_compass, no_trajectory, state_103, wiping_out, grinding, air_flag_452)

struct Compass {
    std::array<float, 9> headings{};  // 240..272 and their working copies280..312
    float movement_heading = 0.0f;     // 316
    float orbit_delta = 0.0f;          // 320
    F4 velocity{};                     // 336
    F4 damped_velocity{};              // 352
    F4 previous_position{};            // 368
    F4 orbit_position{};               // 384
    bool previous_reset = true;        // 400
    float stopped_time = 0.0f;         // 404
    float deadzone_size = 0.0f;        // 408
    std::array<float, 2> deadzone_position{};  // 416
    std::array<float, 2> deadzone_velocity{};  // 432

    static Compass new_() { return Compass{}; }

    ///82DF7B90 supplies0 on subject reset and exactly1/60 otherwise.
    std::array<float, 9> update(float dt, const CompassInputs& input, const CompassSettings& settings);

    // private in Rust
    void update_follow(const CompassInputs& input);
    // ---- compass_movement.rs (pub(super)) ----
    void update_movement(float dt, const CompassInputs& input, const CompassSettings& s);
    void update_deadzone(float dt, std::array<float, 2> target);  // private
    // ---- compass_orbit.rs (pub(super)) ----
    void update_orbit(float dt, const CompassInputs& input);
};
S3_DERIVE_EQ(Compass, headings, movement_heading, orbit_delta, velocity, damped_velocity, previous_position,
             orbit_position, previous_reset, stopped_time, deadzone_size, deadzone_position, deadzone_velocity)

/// pub(super) helpers of compass.rs.
namespace compass {
inline constexpr float PI = rs::from_bits(0x40490fdb);
inline constexpr float TAU = rs::from_bits(0x40c90fdb);
inline F4 horizontal(F4 v) { return {v[0], 0.0f, v[2], v[3]}; }
inline F4 sub(F4 a, F4 b) { return rs::from_fn<4>([&](std::size_t i) { return a[i] - b[i]; }); }
float wrap(float v);
float blend(float from, float to, float weight);
float wrap_floor(float v);
///82DF3888 intentionally tests positive Y only and does not normalize XZ.
float heading(F4 v);
}  // namespace compass

/// Physical fields not already present in ManagerSubject, used by82DF7B90.
struct CompassPoseInputs {
    F4 skeleton_direction{};
    F4 look_target{};
    F4 board_velocity{};
    F4 trajectory_direction{};
    bool state_103 = false;

    CompassInputs bind(const ManagerSubject& subject, F4 camera_position, std::uint32_t selected_compass) const;
};
S3_DERIVE_EQ(CompassPoseInputs, skeleton_direction, look_target, board_velocity, trajectory_direction, state_103)

}  // namespace s3::camera
