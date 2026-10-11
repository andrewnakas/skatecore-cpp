// Port of skate-core/src/camera/shot.rs.
//! Authored normal-camera shots and TU3 interpolation82E06980/82E074A8.
//! The state graph selects stock shot names; these values do not select a view.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <tuple>

#include "s3/core/camera/_types.hpp"

namespace s3::camera {

struct Shot {
    float distance = 2.0f;
    float lens_length = 12.0f;
    /// Direction, elevation, yaw, pitch; native shot68..80.
    std::array<float, 4> smoothing{rs::from_bits(0x3f4ccccd), rs::from_bits(0x3f4ccccd), rs::from_bits(0x3f4ccccd),
                                   rs::from_bits(0x3f4ccccd)};
    /// Native shot84..120 follows Subject's ten reference-point entries.
    std::array<float, 10> reference_weights{0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float board_offset = 0.0f;
    float position_heading = rs::from_bits(0x40490fdb);
    float position_elevation = 0.0f;
    /// Roll, yaw, pitch; native shot136..144, in radians after stock loading.
    std::array<float, 3> framing{};
    std::uint8_t follow_subject_in_air = 1;
    std::uint8_t mirror_for_stance = 1;
    std::uint8_t snap_to_reference_point = 0;
    std::uint8_t use_previous_shot = 0;
    std::uint8_t use_drop_predictor = 0;
    std::uint8_t use_free_camera_stick = 0;
    std::uint8_t avoidance_override = 0;
    float blur = 1.0f;
    float transition_blur = 1.0f;
    float subject_opacity = 1.0f;
    std::uint32_t collision_hint = 0;
    std::uint32_t anchor = 0;
    std::uint32_t compass_north = 0;
    float world_heading = 0.0f;
    F4 arm_orientation{0.0f, 0.0f, 0.0f, 1.0f};
    F4 camera_orientation{0.0f, 0.0f, 0.0f, 1.0f};

    /// Complete Shot constructor82E06230 (the member initialisers above); these initialize
    /// transition history before the first authored shot is loaded, and are not camera tuning.
    static Shot new_() { return Shot{}; }

    /// Normal-shot evaluation82E06CB8/82E00D70. `north` is the subject's
    /// selected compass entry (the manager applies the special 6->5 case).
    /// UsePreviousShot preserves the already copied arm orientation.
    void update_normal(float north, float heading_mirror);

    /// Complete weighted reference-point calculation82E01070. The board
    /// offset follows subject getter456 and is applied to every weighted
    /// reference point before summation, including points with zero weight.
    F4 reference_point(const std::array<F4, 10>& positions, F4 board_offset_direction, F4 fallback) const;

    /// Complete field publication82E06980, also used by82E06E38 after it
    /// applies its first sine-shaped blend. Discrete options come from `to`
    /// even at fraction zero. Heading and shot metadata are not written.
    void interpolate_from(const Shot& from, const Shot& to, float fraction);
};
S3_DERIVE_EQ(Shot, distance, lens_length, smoothing, reference_weights, board_offset, position_heading,
             position_elevation, framing, follow_subject_in_air, mirror_for_stance, snap_to_reference_point,
             use_previous_shot, use_drop_predictor, use_free_camera_stick, avoidance_override, blur,
             transition_blur, subject_opacity, collision_hint, anchor, compass_north, world_heading,
             arm_orientation, camera_orientation)

/// TU3 82E074A8. Style0 eases with native sine; style1 takes the target;
/// other styles preserve the source. The caller owns fraction clamping.
float interpolate_float(float from, float to, std::uint32_t style, float fraction);

namespace shot {
/// pub(super)
float sine_blend(float fraction);
}  // namespace shot

/// Complete blend-value filtering82E07378 after82E07708 produces the raw
/// subject-dependent value. This filter intentionally does not multiply by dt.
float filter_blend_value(float previous, float raw, float smoothing, float minimum, float maximum);

/// Exact82E07240 interval choice. Entries stop at the first absent child;
/// the returned indices may match at the ends. Zero entries publishes nothing.
std::optional<std::tuple<std::size_t, std::size_t, float>> blend_interval(std::array<float, 3> points,
                                                                          std::array<bool, 3> present, float value);

}  // namespace s3::camera
