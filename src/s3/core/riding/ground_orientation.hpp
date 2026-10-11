// Port of skate-core/src/riding/ground_orientation.rs (whole file).
// Riding up-vector and ground-normal calculation in TU3 Reckoning82D8C8F0.
// Inputs are assembled by82D8E5C0; this module owns the three persistent
// filters, target, acceleration and damping. Transform/tilt publication is a
// separate downstream calculation. Numerical primitives remain unverified
// against independent hardware captures.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/air/ground_normal.hpp"
#include "s3/core/math.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::riding::ground_orientation {

using air::ground_normal::GroundNormalFilter;
using math::Vector3;
using point_graph::PointGraph;

inline constexpr Vector3 UP = Vector3{0.0f, 1.0f, 0.0f};
inline constexpr float EPSILON = rs::from_bits(0x358637BDu);  // 82F826F8 installs82181A88 at830BD350.

struct GroundOrientationSettings {
    std::array<float, 4> ground_normal_smoothing{};
    std::array<float, 4> up_vector_smoothing_slow{};
    std::array<float, 4> up_vector_smoothing_fast{};
    PointGraph<8> dynamic_up_vs_ground_y{};
    PointGraph<8> ground_vector_blend{};
    /// The eight x/y pairs inside PointNegGraphData8 at layout256+16.
    /// 82D8CF10 calls ordinary PointGraphEval directly, ignoring its sign mode.
    PointGraph<8> deck_angle_usage_vs_speed{};
    PointGraph<8> up_vector_smoothing_vs_speed{};
    PointGraph<8> up_vector_max_delta_vs_speed{};
    float ground_blend_max_delta = 0.0f;
    float up_vector_max_acceleration = 0.0f;
    float anti_wobble_damping = 0.0f;
    float extra_side_damping = 0.0f;
    std::int32_t minimum_wheels_for_ground_blend = 0;
};
S3_DERIVE_EQ(GroundOrientationSettings, ground_normal_smoothing, up_vector_smoothing_slow,
             up_vector_smoothing_fast, dynamic_up_vs_ground_y, ground_vector_blend,
             deck_angle_usage_vs_speed, up_vector_smoothing_vs_speed, up_vector_max_delta_vs_speed,
             ground_blend_max_delta, up_vector_max_acceleration, anti_wobble_damping,
             extra_side_damping, minimum_wheels_for_ground_blend)

struct GroundOrientationInput {
    /// Processed752, copied from Skeleton11008 in82BD8918.
    Vector3 com_to_deck;
    /// Processed464 <- Ground96 (retained wheel normal).
    Vector3 ground_normal;
    /// Processed528 <- Ground64 (overall normal).
    Vector3 dynamic_up;
    /// Processed2652 <- SkateboardMotion160 (raw deck speed).
    float speed = 0.0f;
    std::int32_t wheel_contact_count = 0;
    /// Processed2720; the native deck-angle gate tests exact zero.
    float animation_balance = 0.0f;
    /// Processed2616 input to DeckAngleUsageVsSpeed.
    float deck_angle_curve_input = 0.0f;
    /// Processed80/96: current board transform Y/Z.
    Vector3 board_up;
    Vector3 board_forward;
    /// Processed160: effective board transform Z.
    Vector3 effective_board_forward;
    /// The previous complete Reckoning transform X (native816), including flip.
    Vector3 previous_reckoning_right;
    /// Processed2476 bit30, the coffin restriction.
    bool prevent_up_behind_board = false;
};
S3_DERIVE_EQ(GroundOrientationInput, com_to_deck, ground_normal, dynamic_up, speed,
             wheel_contact_count, animation_balance, deck_angle_curve_input, board_up, board_forward,
             effective_board_forward, previous_reckoning_right, prevent_up_behind_board)

struct GroundOrientation {
    Vector3 dynamic_up;
    Vector3 up;
    Vector3 target;
    Vector3 up_velocity;
    Vector3 ground_normal;
    float ground_blend = 0.0f;
    // pub(crate) in Rust.
    GroundNormalFilter ground_filter;
    GroundNormalFilter slow_filter;
    GroundNormalFilter fast_filter;

    /// Native82D8DA68 filter history and82D8C3A8/82D8BE38 vector seeds.
    static GroundOrientation new_(const GroundOrientationSettings& settings);
    /// Reset82D8C3A8 resets these vectors/scalars but preserves filter history.
    void reset();
    /// Complete up/normal branch82D8C934..82D8D650, before CalculateTransform,
    /// CalculateDynamicLean and CalculateTilt. BodySpin runs before this stage.
    void update(const GroundOrientationSettings& settings, GroundOrientationInput input);
};
S3_DERIVE_EQ(GroundOrientation, dynamic_up, up, target, up_velocity, ground_normal, ground_blend,
             ground_filter, slow_filter, fast_filter)

/// Complete ClampVectorWithinMaxLength82BD3D90. pub(crate) in Rust.
Vector3 clamp_length(Vector3 value, float maximum);

}  // namespace s3::riding::ground_orientation
