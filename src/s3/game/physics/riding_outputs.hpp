// Port of skate-game/src/physics/riding_outputs.rs.
//! Live board/query/solver outputs used by the riding gameplay owner.
//! Physical calculations stay in core; this owns stock settings and scheduling
//! state. Solved contacts and line probes remain distinct native inputs.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "s3/core/air/body_spin.hpp"
#include "s3/core/graph/conditions.hpp"
#include "s3/core/math.hpp"
#include "s3/core/physics/board_ground.hpp"
#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/board_toolkit.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/core/riding/ground_orientation.hpp"
#include "s3/core/riding/reckoning_frames.hpp"
#include "s3/core/riding/speed_and_slope.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/riding_outputs/ground_input.hpp"
#include "s3/game/physics/riding_outputs/probes.hpp"

namespace s3::game::physics::riding_outputs {

using probes::BoardProbes;

/// Pose-side results which the native Skeleton/complete Reckoning transform own.
/// They cannot be inferred from the board body's center of mass or its up axis.
/// `#[derive(Clone, Copy, Debug)]`.
struct RidingPoseInputs {
    /// Skeleton11008 -> Processed752 in82BD8918.
    sc::math::Vector3 com_to_deck{};
    ///Processed2812, supplied by the actual PhysBodySpin animation attribute.
    float body_spin = 0.0f;
};

struct RidingOutputs {
    sc::physics::board_ground::BoardGroundState ground{};
    sc::physics::board_ground::WheelLineState wheel_lines{};
    BoardProbes probes{};
    sc::riding::ground_orientation::GroundOrientation reckoning{};
    sc::riding::reckoning_frames::ReckoningFrames reckoning_frames{};
    sc::air::body_spin::BodySpinState body_spin{};
    sc::physics::board_motion_output::BoardMotionOutput motion{};
    // Private in Rust.
    sc::riding::ground_orientation::GroundOrientationSettings orientation_settings{};
    sc::point_graph::PointGraph<8> tilt_vs_rotation{};
    sc::point_graph::PointGraph<8> tilt_vs_slope{};
    sc::riding::speed_and_slope::SpeedAndSlopeSettings speed_settings{};
    float maximum_ground_angle = 0.0f;
    float heading_adjust_factor_ = 0.0f;  // Rust field `heading_adjust_factor` (renamed: clashes with the getter)
    std::optional<std::array<std::optional<sc::physics::board_ground::WheelLineHit>, 4>> pending_wheel_queries;

    ///Board82C0D680 resets CollisionInfo and both probes, preserving7692.
    void reset_for_teleport();
    static rs::Result<RidingOutputs, std::string> load(const sd::Collections& data,
                                                       const sc::physics::board_runtime::BoardRuntime& board,
                                                       std::uint32_t processed_flags_2468);

    ///Original82DB5E10 reads current Processed464.y and2612 after Skeleton.
    float update_input_heading(float normal_y, float speed);

    float heading_adjust_factor() const { return heading_adjust_factor_; }

    ///Ground preupdate82D37C50 ->82D38018 submits82D4E250. The host computes
    ///that job directly: its heading seed, BodySpin, up filters, full transform,
    ///dynamic lean and tilt. This must complete before ground forces consume it.
    void update_ground_reckoning(const sc::physics::board_runtime::BoardRuntime& board, RidingPoseInputs pose,
                                 std::uint32_t processed_flags_2468, float animation_balance, bool coffin,
                                 const sc::player::input_phase::ProcessedPhysicsInput& processed);

    /// Animated takeoff retains the rider's heading while the deck begins its
    /// authored spin. Feeding the driven deck axis back into this frame would
    /// apply that spin to the whole skater again on each ground-animation tick.
    void update_ground_reckoning_with_heading(const sc::physics::board_runtime::BoardRuntime& board,
                                              RidingPoseInputs pose, std::uint32_t processed_flags_2468,
                                              float animation_balance, bool coffin,
                                              const sc::player::input_phase::ProcessedPhysicsInput& processed,
                                              std::array<float, 4> heading);

    /// Slide82D3A890 seeds1136/1200 from Processed528/96 and calls82D8C8F0
    /// directly. Its data block is exactly the Processed snapshot82D8E5C0.
    void update_slide_reckoning(const sc::player::input_phase::ProcessedPhysicsInput& p,
                                const sc::physics::board_toolkit::BoardToolkit& toolkit, float physical_body_spin,
                                float balance);

    /// StartSkateboardLineTests82DB6310 ->82C07788: four current wheel positions,
    /// each queried0.2m along negative Reckoning up, radius0 and static-world
    /// backend. World8275EA20 submits these before actor SetUpPhysics.
    Status start_wheel_queries(const sc::physics::board_runtime::BoardRuntime& board,
                               const sc::physics::board_world::BoardWorld& world);

    ///World8275EB10 calls EndBoard8275E6E8 after PlayerInput and before
    ///PostInput/state work. These results are available to this frame's state
    ///selector, independently of the subsequent solve's contact reports.
    Status finish_wheel_queries();

    ///UpdatePostPhysics82C07D20 consumes current solved reports.
    ///Contact-dependent inertia drag affects the next step. ProcessOutput
    ///82C02A80 publishes actual body rates with filtered normal1216.
    Status finish_post_physics(sc::physics::board_runtime::BoardRuntime& board, bool board_wiping_out,
                               std::uint32_t processed_flags_2468, float time_step);

    sc::graph::conditions::SpeedInputs graph_speeds() const;
};

/// Private in Rust.
sc::math::Vector3 vector(std::array<float, 3> v);
/// Private in Rust.
std::array<float, 4> lanes(sc::math::Vector3 v);
/// Private in Rust.
rs::Result<sc::point_graph::PointGraph<8>, std::string> graph(const sd::Collections& data, const std::string& class_,
                                                              const std::string& field);

}  // namespace s3::game::physics::riding_outputs
