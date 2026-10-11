// Port of skate-core/src/air/known/runtime.rs.
// Mandatory engine and Xenon-math boundaries for TU3 KnownAir.
#pragma once
#include <cstdint>
#include <utility>

#include "s3/core/air/known/data.hpp"

namespace s3::air::known::runtime {

using data::AffineTransform;
using data::GrindFootBody;
using data::KnownAirFootplantInput;
using data::KnownAirPrediction;
using data::KnownAirTrajectory;
using data::RestoreVelocityGeometry;
using data::Vector4;

/// Xenon operations whose exact estimate/refinement, lane, or angle behavior
/// is observable in the recovered routines. There is deliberately no default
/// implementation or scalar substitute. (Rust trait -> abstract interface.)
class KnownAirMath {
public:
    virtual ~KnownAirMath() = default;
    /// `vmsum3fp128` including its native rounding and lane behavior.
    virtual float dot3(Vector4 left, Vector4 right) = 0;
    /// The `vrefp` reciprocal refinement sequence used for finite differences.
    virtual Vector4 divide_vector_by_scalar(Vector4 vector, float scalar) = 0;
    /// The two `vrsqrtefp128` refinements, zero selection, and returned length.
    virtual std::pair<Vector4, float> normalize_safe_with_length(Vector4 vector) = 0;
    /// `0x8286CD88`, including its native orientation and degeneracy behavior.
    virtual float signed_angle_about_axis(Vector4 from, Vector4 to, Vector4 axis) = 0;
    /// `0x8258DB98` exactly; callers retain every observed invocation.
    virtual float wrap_angle_8258db98(float angle) = 0;
    /// VMX geometry at `0x82D35A0C..0x82D35B78`. The surrounding trajectory
    /// evaluation, curve evaluation, blend, board write, and speed publication
    /// remain explicit in readable Rust.
    virtual RestoreVelocityGeometry restore_velocity_geometry_82d35998(Vector4 trajectory_velocity,
                                                                       Vector4 ground_normal) = 0;
    /// Affine point transform at `0x82D36AE4..0x82D36B30`.
    virtual Vector4 transform_point_82d36880(AffineTransform transform, Vector4 point) = 0;
};

/// Every external call and direct engine-owned object mutation performed by
/// the recovered KnownAir routines. Implementers must bind all methods.
class KnownAirRuntime : public KnownAirMath {
public:
    virtual void enable_board_angular_drive_only() = 0;
    virtual void set_air_collision_update_enabled(bool enabled) = 0;
    virtual void set_skeleton_collision_state(std::uint32_t state) = 0;
    virtual void set_skeleton_physics_truck_tilt_enabled(bool enabled) = 0;
    virtual void set_skeleton_inverse_kinematics_enabled(bool enabled) = 0;
    virtual void reset_reckoning_flipping() = 0;
    virtual void set_footplant_flag_240(bool value) = 0;
    virtual void reset_footplants() = 0;
    virtual float board_transform_height() = 0;
    virtual Vector4 skeleton_com_position() = 0;

    virtual bool selector_targeting_grind() const = 0;
    virtual void start_grind_air_adjust_from_selector() = 0;
    virtual std::pair<std::int32_t, Vector4> selector_closest_trajectory_point(Vector4 skeleton_com, Vector4 zero) = 0;
    virtual KnownAirPrediction selected_prediction() const = 0;
    /// Selector+2704, distinct from the winning result trajectory at +1680.
    virtual KnownAirTrajectory selector_trajectory_2704() const = 0;
    virtual Vector4 selector_contact_position() = 0;
    virtual Vector4 selector_vector_2848() const = 0;
    virtual Vector4 selector_com_position_2784() const = 0;
    virtual std::pair<Vector4, float> highest_trajectory_position(const KnownAirTrajectory& trajectory) = 0;
    virtual bool selector_has_just_changed() const = 0;
    virtual void reset_trajectory_selector() = 0;

    virtual void begin_reckoning_body_flip(bool side) = 0;
    virtual void update_air_collision() = 0;
    virtual bool grind_air_adjust_started() const = 0;
    virtual void set_grind_air_adjust_started(bool value) = 0;
    virtual void set_grind_air_adjust_activated(bool value) = 0;
    virtual bool grind_air_adjust_adjusting() const = 0;
    virtual void disable_grind_foot_collision(GrindFootBody body) = 0;
    virtual void update_reckoning_air_states(Vector4 landing_normal, float alignment_weight, float body_spin_speed,
                                             float body_flip_speed) = 0;
    virtual void update_known_air_skeleton(Vector4 target_com_position) = 0;
    virtual void set_skeleton_add_skateboard_error(bool value) = 0;
    virtual void set_head_tracking_target(Vector4 target, bool valid) = 0;
    virtual void update_board_steering_tilt(float tilt) = 0;

    virtual void shift_footplant_trajectory_to_index(KnownAirTrajectory& trajectory, std::int32_t index) = 0;
    virtual void update_footplant_prediction(const KnownAirFootplantInput& input) = 0;
    virtual void update_footplant_lock(const KnownAirFootplantInput& input) = 0;
    virtual void update_footplant_pose(const KnownAirFootplantInput& input) = 0;

    virtual Vector4 board_body_velocity() = 0;
    virtual void set_board_velocity(Vector4 velocity) = 0;
    virtual Vector4 board_forward_axis() = 0;
    virtual void check_for_air_wipeout(bool use_com) = 0;
    virtual AffineTransform reckoning_com_transform_816() const = 0;
};

}  // namespace s3::air::known::runtime
