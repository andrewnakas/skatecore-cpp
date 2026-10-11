// Port of skate-core/src/air/known/data.rs.
// Persistent state and direct inputs for TU3 `PhysState_KnownAir`.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/point_graph.hpp"
#include "s3/core/rs.hpp"

namespace s3::air::known::data {

using point_graph::PointGraph;

using Vector4 = std::array<float, 4>;

/// The 64-byte trajectory copied by `FillPhysOut` at `0x82D36A78`.
struct KnownAirTrajectory {
    Vector4 position{};
    Vector4 velocity{};
    Vector4 acceleration{};
    float scalar_48 = 0.0f;
    std::uint32_t word_52 = 0;
    std::uint32_t word_56 = 0;
    std::uint32_t word_60 = 0;
};
S3_DERIVE_EQ(KnownAirTrajectory, position, velocity, acceleration, scalar_48, word_52, word_56, word_60)

/// Winning trajectory result at selector+1680. The collision frame lies at
/// result+128, outside the copied 64-byte trajectory at result+144.
struct KnownAirPrediction {
    KnownAirTrajectory trajectory{};
    /// Winning PredictionResults+48, distinct from request horizon at +192.
    float collision_time_48 = 0.0f;
    std::int32_t collision_frame_128 = 0;
};
S3_DERIVE_EQ(KnownAirPrediction, trajectory, collision_time_48, collision_frame_128)

/// Fields owned by the KnownAir object at native offsets +64..+216.
/// Construction remains caller-owned because Enter does not initialize every
/// field before `InitTrajectoryInfo` and the selector helpers run.
struct KnownAirState {
    Vector4 landing_normal_64{};
    Vector4 landing_heading_80{};
    Vector4 trajectory_apex_96{};
    Vector4 collision_position_112{};
    Vector4 selector_vector_128{};
    Vector4 trajectory_follow_offset_144{};
    Vector4 target_com_position_160{};
    float collision_normal_speed_176 = 0.0f;
    float time_in_state_180 = 0.0f;
    float start_y_184 = 0.0f;
    float max_y_188 = 0.0f;
    float com_max_y_192 = 0.0f;
    float collision_time_196 = 0.0f;
    float time_to_apex_200 = 0.0f;
    float body_flip_target_speed_204 = 0.0f;
    bool reached_apex_208 = false;
    bool landing_heading_valid_209 = false;
    bool start_flipped_210 = false;
    bool body_flipping_211 = false;
    bool grind_air_adjust_activated_212 = false;
    bool targeting_grind_213 = false;
    std::int32_t trajectory_index_216 = 0;
};
S3_DERIVE_EQ(KnownAirState, landing_normal_64, landing_heading_80, trajectory_apex_96, collision_position_112,
             selector_vector_128, trajectory_follow_offset_144, target_com_position_160, collision_normal_speed_176,
             time_in_state_180, start_y_184, max_y_188, com_max_y_192, collision_time_196, time_to_apex_200,
             body_flip_target_speed_204, reached_apex_208, landing_heading_valid_209, start_flipped_210,
             body_flipping_211, grind_air_adjust_activated_212, targeting_grind_213, trajectory_index_216)

/// Direct ProcessedPhysIn fields consumed by KnownAir.
struct KnownAirFrame {
    Vector4 start_flip_reference_96{};
    Vector4 alternate_head_target_112{};
    Vector4 velocity_400{};
    Vector4 ground_normal_464{};
    float start_height_484 = 0.0f;
    Vector4 skater_up_544{};
    std::uint32_t flags_2468 = 0;
    std::uint32_t flags_2472 = 0;
    std::uint32_t flags_2476 = 0;
    std::uint32_t flags_2480 = 0;
    std::uint32_t flags_2484 = 0;
    std::uint32_t flags_2488 = 0;
    std::int32_t next_physics_state_2500 = 0;
    float delta_time_2604 = 0.0f;
    float forward_speed_2612 = 0.0f;
    float body_spin_input_2640 = 0.0f;
    Vector4 selector_landing_normal_2656{};
};
S3_DERIVE_EQ(KnownAirFrame, start_flip_reference_96, alternate_head_target_112, velocity_400, ground_normal_464,
             start_height_484, skater_up_544, flags_2468, flags_2472, flags_2476, flags_2480, flags_2484,
             flags_2488, next_physics_state_2500, delta_time_2604, forward_speed_2612, body_spin_input_2640,
             selector_landing_normal_2656)

/// Cached `physics_airstates/default.xml` values used by this state. Values
/// must come from the stock collection adapter; no retail defaults live here.
struct KnownAirSettings {
    /// Settings +160; x at +176, y at +208.
    PointGraph<8> max_heading_adjust_vs_up_y_160{};
    /// Settings +240; x at +256, y at +288.
    PointGraph<8> landing_speed_scalar_vs_ground_normal_y_240{};
    /// Attribute hash `AA79C93673533C5B`; its recovered name remains unknown.
    PointGraph<8> flip_start_collision_time_vs_normal_y{};
    float trajectory_error_blend_away_time_384 = 0.0f;
    float min_target_heading_velocity_420 = 0.0f;
    float min_auto_body_speed_424 = 0.0f;
    float max_spin_speed_428 = 0.0f;
    float frames_for_grind_air_assist_436 = 0.0f;
    float body_flip_min_grab_time_fraction_456 = 0.0f;
    /// `physics_reckoning/FlipScalar`, hash `978CE73B0EFE76C8`.
    float flip_scalar = 0.0f;
};
S3_DERIVE_EQ(KnownAirSettings, max_heading_adjust_vs_up_y_160, landing_speed_scalar_vs_ground_normal_y_240,
             flip_start_collision_time_vs_normal_y, trajectory_error_blend_away_time_384,
             min_target_heading_velocity_420, min_auto_body_speed_424, max_spin_speed_428,
             frames_for_grind_air_assist_436, body_flip_min_grab_time_fraction_456, flip_scalar)

/// Fields read through ProcessedPhysIn+2548 and its selected physics mode.
struct KnownAirModeSettings {
    bool upside_down_falling_wipeout_enabled_2 = false;
    bool perfect_body_flips_28 = false;
    float body_spin_speed_limit_56 = 0.0f;
};
S3_DERIVE_EQ(KnownAirModeSettings, upside_down_falling_wipeout_enabled_2, perfect_body_flips_28,
             body_spin_speed_limit_56)

struct KnownAirWipeoutSettings {
    float air_falling_min_up_y_260 = 0.0f;
    float air_falling_max_angle_264 = 0.0f;
};
S3_DERIVE_EQ(KnownAirWipeoutSettings, air_falling_min_up_y_260, air_falling_max_angle_264)

/// Direct Reckoning fields read or cleared by KnownAir.
struct KnownAirReckoningFields {
    Vector4 landing_normal_1152{};
    Vector4 heading_axis_1200{};
    float body_spin_angle_1568 = 0.0f;
    float body_spin_speed_1572 = 0.0f;
};
S3_DERIVE_EQ(KnownAirReckoningFields, landing_normal_1152, heading_axis_1200, body_spin_angle_1568,
             body_spin_speed_1572)

/// Direct writes to the wipeout request object at +35, +116, and +200.
struct KnownAirWipeoutRequest {
    bool requested_35 = false;
    float scalar_116 = 0.0f;
    std::uint32_t counter_200 = 0;
};
S3_DERIVE_EQ(KnownAirWipeoutRequest, requested_35, scalar_116, counter_200)

/// Six skeleton bodies disabled, in this exact native order, while the grind
/// air adjustment owns the feet.
enum class GrindFootBody {
    Body3152,
    Body3156,
    Body3160,
    Body3168,
    Body3172,
    Body3176,
};

/// Trajectory packet passed to the three recovered footplant updates.
struct KnownAirFootplantInput {
    KnownAirTrajectory trajectory{};
    float remaining_collision_time = 0.0f;
    Vector4 collision_position{};
    Vector4 landing_normal{};
};
S3_DERIVE_EQ(KnownAirFootplantInput, trajectory, remaining_collision_time, collision_position, landing_normal)

/// Native transform at Reckoning+816, interpreted only by the required math
/// adapter because Xenon lane order remains part of that contract.
struct AffineTransform {
    std::array<Vector4, 4> vectors{};
};
S3_DERIVE_EQ(AffineTransform, vectors)

/// Stable inputs/outputs around the unresolved VMX geometry in `0x82D35998`.
struct RestoreVelocityGeometry {
    Vector4 tangential_velocity{};
    float landing_speed_curve_input = 0.0f;
    /// Source clamped to [0, 1] by the two native `fsel` operations.
    float landing_speed_blend_source = 0.0f;
};
S3_DERIVE_EQ(RestoreVelocityGeometry, tangential_velocity, landing_speed_curve_input, landing_speed_blend_source)

/// Fields written by `FillPhysOut` (`0x82D36880`). The two conditional fields
/// retain their previous value when flags+2468 bit 3 is clear.
struct KnownAirOutput {
    Vector4 trajectory_apex_0{};
    Vector4 collision_position_16{};
    Vector4 landing_normal_32{};
    Vector4 selector_vector_48{};
    Vector4 trajectory_position_64{};
    Vector4 landing_heading_80{};
    Vector4 selector_com_position_96{};
    Vector4 landing_normal_copy_144{};
    Vector4 locked_trajectory_velocity_160{};
    float time_in_state_176 = 0.0f;
    float collision_time_180 = 0.0f;
    float time_until_collision_184 = 0.0f;
    float collision_normal_speed_188 = 0.0f;
    float time_to_apex_196 = 0.0f;
    float jump_height_200 = 0.0f;
    std::int32_t trajectory_index_220 = 0;
    KnownAirTrajectory selected_trajectory_240{};
    std::array<float, 25> trajectory_plane_samples_336{};
    bool reached_apex_436 = false;
    bool known_air_valid_437 = false;
    bool locked_trajectory_velocity_valid_452 = false;
};
S3_DERIVE_EQ(KnownAirOutput, trajectory_apex_0, collision_position_16, landing_normal_32, selector_vector_48,
             trajectory_position_64, landing_heading_80, selector_com_position_96, landing_normal_copy_144,
             locked_trajectory_velocity_160, time_in_state_176, collision_time_180, time_until_collision_184,
             collision_normal_speed_188, time_to_apex_196, jump_height_200, trajectory_index_220,
             selected_trajectory_240, trajectory_plane_samples_336, reached_apex_436, known_air_valid_437,
             locked_trajectory_velocity_valid_452)

}  // namespace s3::air::known::data
