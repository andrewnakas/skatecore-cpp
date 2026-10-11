// Port of skate-core/src/air/state/data.rs.
// Persistent fields and direct inputs used by TU3 `PhysState_PhysicsAir`.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/point_graph.hpp"
#include "s3/core/rs.hpp"

namespace s3::air::state::data {

using point_graph::PointGraph;

using Vector4 = std::array<float, 4>;

/// Native trajectory storage at PhysicsAir+80.
struct AirTrajectory {
    Vector4 position{};
    Vector4 velocity{};
    Vector4 acceleration{};
    /// Native trajectory +48. Both recovered initializers write -1.0.
    float scalar_48 = 0.0f;
};
S3_DERIVE_EQ(AirTrajectory, position, velocity, acceleration, scalar_48)

/// Fields owned by the TU3 PhysicsAir object. Enter preserves prior trajectory
/// storage when COM mode is not selected; construct once and retain this owner.
///
/// Default (original constructor82D341D0): zero vectors80/96/112, -1 at128,
/// up144, zero160/164/168 and172..176. Entry is a separate operation.
struct PhysicsAirState {
    /// PhysicsAir+80.
    AirTrajectory centre_of_mass_trajectory{Vector4{}, Vector4{}, Vector4{}, -1.0f};
    /// PhysicsAir+144.
    Vector4 landing_normal{0.0f, 1.0f, 0.0f, 0.0f};
    /// PhysicsAir+160.
    float time_in_state = 0.0f;
    /// PhysicsAir+164/+168.
    float start_y = 0.0f;
    float max_y = 0.0f;
    /// PhysicsAir+172/+173/+174.
    bool reached_apex = false;
    bool use_centre_of_mass_velocity = false;
    /// The producer is selector+9662. A more specific stock name is unresolved.
    bool selector_latch_174 = false;
    /// PhysicsAir+176, decremented as a signed wrapping integer.
    std::int32_t trajectory_query_countdown = 0;
};
S3_DERIVE_EQ(PhysicsAirState, centre_of_mass_trajectory, landing_normal, time_in_state, start_y, max_y, reached_apex,
             use_centre_of_mass_velocity, selector_latch_174, trajectory_query_countdown)

/// Direct ProcessedPhysIn values consumed by the recovered methods.
struct PhysicsAirFrame {
    Vector4 current_velocity_400{};
    /// Y lane of the all-states ground position, read directly at +500.
    float ground_position_y_500 = 0.0f;
    Vector4 trajectory_position_592{};
    Vector4 trajectory_velocity_608{};
    Vector4 jump_velocity_848{};
    std::uint32_t flags_2468 = 0;
    std::int32_t previous_physics_state_2504 = 0;
    std::int32_t previous_physics_category_2516 = 0;
    std::int32_t frames_since_jump_correction_2576 = 0;
    float delta_time_2604 = 0.0f;
    float body_spin_input_2640 = 0.0f;
    float gravity_y_2648 = 0.0f;
    float state_timer_2664 = 0.0f;
};
S3_DERIVE_EQ(PhysicsAirFrame, current_velocity_400, ground_position_y_500, trajectory_position_592,
             trajectory_velocity_608, jump_velocity_848, flags_2468, previous_physics_state_2504,
             previous_physics_category_2516, frames_since_jump_correction_2576, delta_time_2604,
             body_spin_input_2640, gravity_y_2648, state_timer_2664)

/// Cached collection values read at PhysicsAir settings +320..+444.
/// No retail defaults are embedded; the stock vault adapter must provide them.
struct PhysicsAirSettings {
    /// PointGraph x at +320 and y at +352.
    PointGraph<8> body_spin_over_time_320{};
    float landing_normal_blend_388 = 0.0f;
    float body_spin_scale_428 = 0.0f;
    float landing_normal_angle_limit_444 = 0.0f;
};
S3_DERIVE_EQ(PhysicsAirSettings, body_spin_over_time_320, landing_normal_blend_388, body_spin_scale_428,
             landing_normal_angle_limit_444)

/// Reckoning fields read directly by PhysicsAir, apart from the full
/// `Reckoning::UpdateAirStates` effect exposed by the runtime interface.
struct PhysicsAirReckoningFields {
    Vector4 current_landing_normal_1152{};
    Vector4 collision_reference_normal_1216{};
    float body_spin_angle_1568 = 0.0f;
    float body_spin_speed_1572 = 0.0f;
};
S3_DERIVE_EQ(PhysicsAirReckoningFields, current_landing_normal_1152, collision_reference_normal_1216,
             body_spin_angle_1568, body_spin_speed_1572)

/// Native zero-initialized point-force record passed to `0x82D944E8`.
struct AirBoardForce {
    Vector4 force_world{};
    Vector4 point_board_local{};

    static const AirBoardForce ZERO;
};
inline constexpr AirBoardForce AirBoardForce::ZERO = AirBoardForce{Vector4{}, Vector4{}};
S3_DERIVE_EQ(AirBoardForce, force_world, point_board_local)

/// Writes performed by `FillPhysOut` (`0x82D34E90`). `scalar_184_write` is
/// `None` when native code leaves the existing PhysOut field untouched.
struct PhysicsAirOutput {
    bool is_at_apex = false;
    float jump_height = 0.0f;
    Vector4 landing_normal{};
    std::optional<float> scalar_184_write;
};
S3_DERIVE_EQ(PhysicsAirOutput, is_at_apex, jump_height, landing_normal, scalar_184_write)

}  // namespace s3::air::state::data
