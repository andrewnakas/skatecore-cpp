// Port of skate-core/src/air/state/runtime.rs.
// Mandatory engine-facing effects used by TU3 PhysicsAir.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/air/state/data.hpp"
#include "s3/core/air/state/jump_velocity.hpp"

namespace s3::air::state::runtime {

using data::AirBoardForce;
using data::PhysicsAirReckoningFields;
using data::Vector4;
using jump_velocity::PhysicsAirMath;

/// Mutations of the 272-byte trajectory launch record recovered at
/// `0x82D33448`, `0x82BE33D0`, and `0x82D34570`.
/// (Rust trait -> abstract interface.)
class PhysicsAirLaunchInfo {
public:
    virtual ~PhysicsAirLaunchInfo() = default;
    virtual void set_start_velocity(Vector4 velocity) = 0;
    virtual Vector4 centre_of_mass_animation_position() const = 0;
    virtual void set_trajectory_start_position_override(Vector4 position) = 0;
    virtual void set_board_position_override(Vector4 position) = 0;
    virtual std::array<float, 2> cone_angles() const = 0;
    virtual void set_cone_angles(std::array<float, 2> angles) = 0;
    virtual void set_player_jumped(bool jumped) = 0;
    virtual void set_use_trajectory_start_position_override(bool enabled) = 0;
    virtual void set_trajectory_count(std::uint16_t count) = 0;
};

/// Every external call/read made by the recovered PhysicsAir methods. There is
/// deliberately no no-op/default implementation: an engine adapter must bind
/// each stock-owned object before this state can be scheduled.
///
/// The Rust associated type `LaunchInfo` is the template parameter; it must
/// derive from PhysicsAirLaunchInfo.
template <class LaunchInfoT>
class PhysicsAirRuntime : public PhysicsAirMath {
public:
    using LaunchInfo = LaunchInfoT;

    virtual void set_air_collision_update_enabled(bool enabled) = 0;
    virtual void set_skeleton_collision_state(std::uint32_t state) = 0;
    virtual void set_skeleton_inverse_kinematics_enabled(bool enabled) = 0;
    virtual void enable_board_angular_drive_only() = 0;
    virtual void set_footplant_flag_240(bool value) = 0;
    virtual void reset_footplants() = 0;
    virtual float board_transform_height() = 0;
    virtual void request_skeleton_heading_update() = 0;

    virtual void update_air_collision() = 0;
    virtual bool trajectory_query_just_started() const = 0;
    virtual LaunchInfo construct_trajectory_launch_info() = 0;
    virtual void fill_skeleton_launch_info(LaunchInfo& info) = 0;
    virtual void launch_trajectory(const LaunchInfo& info) = 0;
    virtual void update_trajectory_selector() = 0;
    virtual std::optional<Vector4> selector_landing_normal() const = 0;
    virtual bool selector_centre_of_mass_trajectory_ready() const = 0;

    /// `AngleBetweenVectors` (`0x8296EBB0`), including its unresolved VMX
    /// normalization behavior. The caller performs the recovered angle wrap.
    virtual float angle_between_vectors(Vector4 left, Vector4 right) = 0;
    /// Return the completed canonical fields: UpdateSkateboard reads the newly
    /// filtered collision normal1216, after82D8DBD8, not the entry snapshot.
    virtual PhysicsAirReckoningFields update_reckoning_air_states(Vector4 landing_normal, float normal_blend,
                                                                  float body_spin, float body_flip) = 0;
    virtual void update_known_air_skeleton(Vector4 trajectory_position) = 0;
    virtual void update_animated_skateboard_skeleton(bool argument) = 0;

    virtual void update_board_steering_tilt(float tilt) = 0;
    /// `Toolkit_CalcCollisionForce` (`0x82D944E8`). The adapter supplies its
    /// bound settings and complete live ProcessedPhysIn; PhysicsAir only passes
    /// the reference normal and receives the point-force record.
    virtual bool calculate_air_collision_force(Vector4 reference_normal, AirBoardForce& force) = 0;
    virtual void enqueue_board_force(std::uint32_t tag, AirBoardForce force) = 0;
    virtual void set_board_velocity(Vector4 velocity) = 0;
    virtual void enable_skateboard_error_on_skeleton() = 0;

    virtual Vector4 board_body_velocity() = 0;
    virtual void check_for_air_wipeout(bool argument) = 0;
};

}  // namespace s3::air::state::runtime
