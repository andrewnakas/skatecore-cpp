// Port of skate-game/src/physics/known_air/runtime.rs.
//! Concrete KnownAir boundary. All callbacks mutate the same live owners.
//! Fallible host queries/pose checks retain their first error for the phase caller.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "s3/core/air/known.hpp"
#include "s3/core/air/trajectory.hpp"
#include "s3/core/physics/board_toolkit.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"
#include "s3/game/physics/known_air/math.hpp"

namespace s3::game::physics::known_air::runtime {

using math::V;
using ::s3::air::known::AffineTransform;
using ::s3::air::known::GrindFootBody;
using ::s3::air::known::KnownAirFootplantInput;
using ::s3::air::known::KnownAirPrediction;
using ::s3::air::known::KnownAirTrajectory;
using ::s3::air::known::RestoreVelocityGeometry;
using ::s3::air::trajectory::Selection;
using ::s3::air::trajectory::Trajectory;
using ::s3::physics::board_toolkit::BoardToolkit;

/// `struct Runtime<'a>` + `impl KnownAirRuntime for Runtime<'_>` (+ `impl KnownAirMath`, math.rs;
/// + `impl Runtime<'_>` reset_flip/begin_flip, flip.rs).
struct Runtime final : ::s3::air::known::KnownAirRuntime {
    GamePhysics& physics;
    SkaterRuntime& skater;
    BoardToolkit toolkit;
    // Private in Rust.
    Selection selection;
    V local_com{};
    V board_position{};
    std::optional<std::string> error;

    Runtime(GamePhysics& physics_, SkaterRuntime& skater_, BoardToolkit toolkit_, Selection selection_, V local_com_,
            V board_position_)
        : physics(physics_),
          skater(skater_),
          toolkit(toolkit_),
          selection(selection_),
          local_com(local_com_),
          board_position(board_position_) {}

    static rs::Result<Runtime, std::string> new_(GamePhysics& physics, SkaterRuntime& skater);
    /// Consumes `self` in Rust.
    Status finish();
    // Private in Rust.
    void record(const Status& result);

    // ---- flip.rs (`impl Runtime<'_>`) ----
    void reset_flip();
    void begin_flip(bool side);

    // ---- math.rs (`impl KnownAirMath for Runtime<'_>`) ----
    float dot3(V a, V b) override;
    V divide_vector_by_scalar(V v, float s) override;
    std::pair<V, float> normalize_safe_with_length(V v) override;
    float signed_angle_about_axis(V a, V b, V axis) override;
    float wrap_angle_8258db98(float a) override;
    RestoreVelocityGeometry restore_velocity_geometry_82d35998(V v, V n) override;
    V transform_point_82d36880(AffineTransform m, V p) override;

    // ---- runtime.rs (`impl KnownAirRuntime for Runtime<'_>`) ----
    void enable_board_angular_drive_only() override;
    void set_air_collision_update_enabled(bool v) override;
    void set_skeleton_collision_state(std::uint32_t state) override;
    void set_skeleton_physics_truck_tilt_enabled(bool v) override;
    void set_skeleton_inverse_kinematics_enabled(bool v) override;
    void reset_reckoning_flipping() override;
    void set_footplant_flag_240(bool v) override;
    void reset_footplants() override;
    float board_transform_height() override;
    V skeleton_com_position() override;
    bool selector_targeting_grind() const override;
    void start_grind_air_adjust_from_selector() override;
    std::pair<std::int32_t, V> selector_closest_trajectory_point(V com, V zero) override;
    KnownAirPrediction selected_prediction() const override;
    KnownAirTrajectory selector_trajectory_2704() const override;
    V selector_contact_position() override;
    V selector_vector_2848() const override;
    V selector_com_position_2784() const override;
    std::pair<V, float> highest_trajectory_position(const KnownAirTrajectory& t) override;
    bool selector_has_just_changed() const override;
    void reset_trajectory_selector() override;
    void begin_reckoning_body_flip(bool side) override;
    void update_air_collision() override;
    bool grind_air_adjust_started() const override;
    void set_grind_air_adjust_started(bool v) override;
    void set_grind_air_adjust_activated(bool v) override;
    bool grind_air_adjust_adjusting() const override;
    void disable_grind_foot_collision(GrindFootBody body) override;
    void update_reckoning_air_states(V n, float blend, float spin, float flip) override;
    void update_known_air_skeleton(V target) override;
    void set_skeleton_add_skateboard_error(bool v) override;
    void set_head_tracking_target(V target, bool valid) override;
    void update_board_steering_tilt(float tilt) override;
    void shift_footplant_trajectory_to_index(KnownAirTrajectory& t, std::int32_t index) override;
    void update_footplant_prediction(const KnownAirFootplantInput& input) override;
    void update_footplant_lock(const KnownAirFootplantInput& input) override;
    void update_footplant_pose(const KnownAirFootplantInput& input) override;
    V board_body_velocity() override;
    void set_board_velocity(V v) override;
    V board_forward_axis() override;
    void check_for_air_wipeout(bool use_com) override;
    AffineTransform reckoning_com_transform_816() const override;
};

KnownAirTrajectory trajectory(Trajectory t);
// Private in Rust.
Trajectory actual(const KnownAirTrajectory& t);

}  // namespace s3::game::physics::known_air::runtime
