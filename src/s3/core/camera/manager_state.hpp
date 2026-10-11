// Port of skate-core/src/camera/manager_state.rs. The ManagerState methods defined in
// manager_air.rs, manager_controls.rs, manager_incline.rs and manager_rig.rs are declared
// here and implemented in the matching .cpp files.
//! Normal CameraMan persistent state, constructor/reset82DFDA60/82DFDFA0.
#pragma once
#include <cstdint>
#include <optional>
#include <utility>

#include "s3/core/camera/_types.hpp"
#include "s3/core/camera/look_input.hpp"
#include "s3/core/camera/tracker.hpp"

namespace s3::camera {

struct Rig;
struct ManagerSubject;
struct ShotManager;
struct RigOrientation;
struct DropPredictor;
struct Shot;

namespace manager_state {
inline constexpr float STEP = rs::from_bits(0x3c888889);
inline constexpr float HALF_PI = rs::from_bits(0x3fc90fdb);
inline constexpr float DEGREES = rs::from_bits(0x42652ee1);
inline constexpr F4 UP = {0.0f, 1.0f, 0.0f, 0.0f};

inline float clamp(float value, float low, float high) {
    value = low - value >= 0.0f ? low : value;
    return high - value >= 0.0f ? value : high;
}

inline ScalarTracker mirror_tracker() { return ScalarTracker{1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f}; }

inline ScalarTrackerParameters mirror_parameters(float speed, float acceleration) {
    ScalarTrackerParameters p;
    p.delta_umbra = 0.0f;
    p.delta_penumbra = 0.0f;
    p.speed_clamp = speed;
    p.acceleration_clamp_min = acceleration;
    p.acceleration_clamp_max = acceleration;
    p.smoothing_min = 0.85f;
    p.smoothing_max = 0.85f;
    p.overshoot_zeroes_velocity = true;
    return p;
}
}  // namespace manager_state

struct ManagerState {
    F4 anchor{};                               // 624
    F4 anchor_velocity{};                      // 656
    F4 landing_normal = manager_state::UP;     // 720
    F4 launch_position{};                      // 736
    F4 ground_normal = manager_state::UP;      // 752
    F4 previous_velocity{};                    // 768
    F4 incline_normal{};                       // 784
    std::uint32_t selected_anchor = 0;         // 800
    float frontside_angle = 0.0f;              // 808
    float ground_heading = 0.0f;               // 812
    float velocity_incline = 0.0f;             // 816
    float absolute_velocity_incline = 0.0f;    // 820
    float launch_incline = 0.0f;               // 824
    float signed_landing_incline = 0.0f;       // 828
    float landing_incline = 0.0f;              // 832
    float direction_incline = 0.0f;            // 836
    float field_of_view = 0.0f;                // 844
    float apex_height = 0.0f;                  // 848
    float apex_time = 0.0f;                    // 852
    float time_without_trajectory = rs::f32::MAX;  // 856
    float landing_height_delta = 0.0f;         // 860
    float apex_height_ratio = 0.0f;            // 864
    float launch_landing_heading_delta = 0.0f; // 868
    float trajectory_camera_distance = 0.0f;   // 872
    float steering_time = 0.0f;                // 876
    float centred_time = 0.0f;                 // 880
    float opacity = 1.0f;                      // 884
    float aspect_ratio = rs::from_bits(0x3fe38e39);  // 888
    float blur = 1.0f;                         // 896
    float effect_weight = 1.0f;                // 900
    float collision_elevation = 0.0f;          // 904
    std::uint32_t frames = 0;                  // 908
    std::uint32_t slowmo_frames = 0;           // 912
    std::uint32_t instant_frames = 3;          // 916
    std::uint8_t flags = 2;                    // 920
    std::uint8_t options = 0x60;               // 921
    ScalarTracker heading_mirror = manager_state::mirror_tracker();  // 464
    ScalarTracker framing_mirror = manager_state::mirror_tracker();  // 524

    static ManagerState new_() { return ManagerState{}; }

    bool reset_requested() const { return (this->options & 0x80) != 0; }
    void request_reset() { this->options |= 0x80; }
    bool mirrored() const { return (this->flags & 8) != 0; }

    // ---- manager_air.rs (pub(super)) ----
    /// Returns the exact normal/velocity pair passed to ShakeEffect82E055D8
    /// when the native ground-camera transition triggers a landing impulse.
    std::optional<std::pair<F4, F4>> update_air(const ManagerSubject& subject, Rig& rig);
    void update_slope_relation(const ManagerSubject& subject);  // private in Rust

    // ---- manager_controls.rs (pub(super)) ----
    void update_timing(const ManagerSubject& subject, float steering_threshold);
    void update_mirrors(float dt, const ManagerSubject& subject, const Rig& rig, const ShotManager& shots);

    // ---- manager_incline.rs (pub(super)) ----
    void update_incline(const ManagerSubject& subject);

    // ---- manager_rig.rs (pub(super)) ----
    float bind_shot(const Shot& shot, const ManagerSubject& subject, Rig& rig, RigOrientation& orientation,
                    const DropPredictor& drop, LookInput look, float previous_roll, bool transitioning);
};
S3_DERIVE_EQ(ManagerState, anchor, anchor_velocity, landing_normal, launch_position, ground_normal,
             previous_velocity, incline_normal, selected_anchor, frontside_angle, ground_heading, velocity_incline,
             absolute_velocity_incline, launch_incline, signed_landing_incline, landing_incline, direction_incline,
             field_of_view, apex_height, apex_time, time_without_trajectory, landing_height_delta,
             apex_height_ratio, launch_landing_heading_delta, trajectory_camera_distance, steering_time,
             centred_time, opacity, aspect_ratio, blur, effect_weight, collision_elevation, frames, slowmo_frames,
             instant_frames, flags, options, heading_mirror, framing_mirror)

}  // namespace s3::camera
