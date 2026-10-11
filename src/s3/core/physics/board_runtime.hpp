// Port of skate-core/src/physics/board_runtime.rs.
//
// Persistent board physics for the host game.
//
// Ownership and spawning are host policy. Physical initialization follows TU3
// AddRigidBody 82AE5928 and Reset 82776140: dynamic bodies start at rest with
// gravity accumulated, while the separate hook has zero inverse mass/tensor.
// Part pose conversion reuses 82BD4318/82C0B2C8; no packed body is retained
// beside the state consumed and updated by BoardStep.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "s3/core/math.hpp"
#include "s3/core/physics/assembly.hpp"
#include "s3/core/physics/board.hpp"
#include "s3/core/physics/board_pose.hpp"
#include "s3/core/physics/board_step.hpp"
#include "s3/core/physics/contact_feedback.hpp"
#include "s3/core/physics/contact_solver.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/physics/force_queue.hpp"
#include "s3/core/physics/hook_drive.hpp"
#include "s3/core/physics/rigid_body.hpp"
#include "s3/core/physics/slice.hpp"

namespace s3::physics::board_runtime {

using assembly::BoardHook;
using assembly::BodySnapshot;
using board::BODY_COUNT;
using board::BodyId;
using board_pose::PartPose;
using board_step::AttachedStep;
using board_step::BoardCollision;
using board_step::BoardStep;
using board_step::BoardStepSettings;
using drive_frames::RetailAffineTransform;
using force_queue::BoardForceQueue;
using math::Basis3;
using math::Vector3;
using rigid_body::RetailBodyMassProperties;
using rigid_body::RetailInertiaDynamics;
using rigid_body::RetailLocalMassFrame;
using rigid_body::RetailSimulationStep;

/// The host explicitly chooses the board's initial physical mode.
enum class BoardMotion : std::uint8_t {
    Active,
    Frozen,
    Static,
};

class BoardRuntime {
public:
    /// Opt-in inspection of completed rows; capture never changes solver inputs.
    std::optional<std::string> take_solver_diagnostics(bool enabled);

    /// `authored` contains part poses in board space. `spawn` places the deck
    /// part in world space, and moves the other parts by the same rigid delta.
    /// All mass properties come from the caller's physical data producer.
    static BoardRuntime new_(std::array<RetailBodyMassProperties, BODY_COUNT> masses,
                             std::array<RetailAffineTransform, BODY_COUNT> authored, RetailAffineTransform spawn,
                             RetailSimulationStep simulation, BoardMotion mode);

    std::uint32_t collision_group() const { return collision_group_; }
    /// All seven native board parts share the current assembly group.
    void set_collision_group(std::uint32_t group) { collision_group_ = group; }

    const std::array<BodySnapshot, BODY_COUNT>& bodies() const { return bodies_; }

    /// Physical portion of82C05F50/82C0D680. Restore the authored part poses,
    /// apply the requested stance and board transform, then clear rates. State
    /// flags, cooldown, inertias and target-body rates retain their own owners.
    void reset_physical(std::array<RetailAffineTransform, BODY_COUNT> authored, RetailAffineTransform target,
                        std::uint32_t processed_flags, Vector3 gravity);

    /// Move the complete board assembly while preserving live rates, forces,
    /// contacts, and solver ownership. This is the C055F0/C0B2C8 possession
    /// position operation, distinct from the full physical reset above.
    void set_transform(RetailAffineTransform target);

    /// Gameplay may publish actual velocities/accumulators or physical mode
    /// changes here. Rendering should use the immutable view or part poses.
    std::array<BodySnapshot, BODY_COUNT>& bodies_mut() { return bodies_; }

    const BoardHook& hook() const { return hook_; }

    /// Animation/state services own the hook target and live drive parameters.
    BoardHook& hook_mut() { return hook_; }

    const BoardForceQueue& forces() const { return forces_; }

    Slice<const contact_feedback::BoardContactReport> contact_reports() const { return step_.contact_reports(); }

    /// Original post-solver observers consume these completed shared rows.
    Slice<const contact_solver::RetailContactJacobian> solved_contacts() const { return step_.solved_contacts(); }

    BoardForceQueue& forces_mut() { return forces_; }

    /// Queue clearing is a separate gameplay phase, as in GeneralUpdate
    /// 82C02360. Advancing physics does not silently consume its ownership.
    void clear_forces() { forces_.clear(); }

    void advance(Slice<const BoardCollision> collisions, std::array<float, 2> truck_targets,
                 BoardStepSettings settings);

    /// The skater, board and hook constraints exchange reactions in every
    /// iteration; this never advances the skeleton in a separate solver.
    void advance_attached(Slice<const BoardCollision> collisions, std::array<float, 2> truck_targets,
                          BoardStepSettings settings, AttachedStep attached);

    /// Live center-of-mass pose for collision and other physical consumers.
    RetailAffineTransform body_transform(BodyId id) const;

    /// Live visual/collision part poses, including each mass-frame offset.
    /// This derives poses from the integrated bodies rather than stale spawn
    /// matrices, and never writes physics from presentation.
    std::array<RetailAffineTransform, BODY_COUNT> part_transforms() const;

    RetailAffineTransform hook_transform() const;

    /// Publish an animation target while retaining the hook's zero physical
    /// response and its separate drive state.
    void set_hook_transform(RetailAffineTransform requested);

private:
    BoardRuntime() = default;
    std::array<BodySnapshot, BODY_COUNT> bodies_{};
    std::array<RetailLocalMassFrame, BODY_COUNT> mass_frames_{};
    BoardHook hook_{};
    BoardStep step_{};
    BoardForceQueue forces_{};
    /// Original assembly/part group: standard82C090C0=4, wipeout82C09160=7.
    std::uint32_t collision_group_ = 0;
};

/// Rust-private, exposed for the ported tests (`mode.flags()`).
std::uint32_t flags(BoardMotion mode);

/// Rust-private, exposed for the ported tests.
inline constexpr RetailInertiaDynamics ZERO_INERTIA = RetailInertiaDynamics{
    Vector3{0.0f, 0.0f, 0.0f}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
};

/// pub(crate)
BodySnapshot initialized_body(const PartPose& part, RetailInertiaDynamics inertia, RetailSimulationStep simulation,
                              BoardMotion mode);

}  // namespace s3::physics::board_runtime
