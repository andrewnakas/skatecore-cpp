// Port of skate-core/src/physics/board_step.rs.
//
// The connected-board path through TU3's simulation frame.
//
// Sources: simulation_job_setup_resolution.json (82DC35E0),
// drive_solver_handoff_resolution.json (82DC3098 / 82AE27D0 / 82AE6590).
// This owns compilation, shared reactions and integration. Collision queries
// and ground/action-state force production have separate owners.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "s3/core/math.hpp"
#include "s3/core/physics/assembly.hpp"
#include "s3/core/physics/board.hpp"
#include "s3/core/physics/board_step/collision_body.hpp"
#include "s3/core/physics/contact.hpp"
#include "s3/core/physics/contact_feedback.hpp"
#include "s3/core/physics/contact_solver.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/physics/drive_parameters.hpp"
#include "s3/core/physics/drive_solver.hpp"
#include "s3/core/physics/force_queue.hpp"
#include "s3/core/physics/rigid_body.hpp"
#include "s3/core/physics/slice.hpp"
#include "s3/core/physics/solver.hpp"

namespace s3::physics::board_step {

using assembly::BoardHook;
using assembly::BodySnapshot;
using contact::RetailContactInput;
using contact_feedback::BoardContactReport;
using contact_solver::RetailContactJacobian;
using drive_frames::RetailAffineTransform;
using drive_parameters::RetailDriveDynamics;
using drive_solver::RetailDriveRows;
using force_queue::BoardForceQueue;
using math::Vector3;
using rigid_body::RetailReactionCorrections;
using rigid_body::RetailSimulationStep;
using solver::JointConstraint;

/// #[cfg(test)] constants, exposed for the ported tests.
inline constexpr std::size_t WORLD_REACTION = BODY_COUNT + 1;
inline constexpr std::size_t REACTION_COUNT = BODY_COUNT + 2;

/// Other assemblies participating in this SAME solve. References point to the
/// actual bodies owned by the physical skeleton and its hooks. The caller builds
/// their rows after publishing this tick's physical drive targets and forces.
struct AttachedStep {
    std::vector<BodySnapshot*> bodies;
    Slice<RetailContactJacobian> contacts;
    Slice<JointConstraint> joints;
    Slice<RetailDriveRows> drives;

    std::size_t world_reaction() const { return ATTACHED_REACTION_BASE + bodies.size(); }
};

/// Narrow-phase output, in the collision provider's original A/B order.
/// Materials must already be combined by `combine_contact_materials`.
struct BoardCollision {
    CollisionBody body_a;
    CollisionBody body_b;
    RetailContactInput contact;
};

struct BoardStepSettings {
    RetailSimulationStep simulation;
    /// Loaded from Simulation+176 by 82DC3098. Not a fixed solver constant.
    std::uint32_t iterations = 0;
    std::array<RetailAffineTransform, 2> base_truck_transforms{};
    RetailDriveDynamics truck_dynamics;
    float force_point_y_offset = 0.0f;
};

/// Reuses the contact workspace between ticks. Seven board bodies and the
/// separate hook share solver reactions. This does not assign islands or implement the
/// native activation partition for multiple disconnected assemblies.
///
/// C++: the Rust-private workspaces are public members so the ported tests (which
/// read them through `use super::*`) can inspect them; callers should treat them
/// as private.
struct BoardStep {
    std::vector<RetailContactJacobian> contacts;
    std::vector<BoardContactReport> reports;
    std::vector<RetailReactionCorrections> reactions;
    bool diagnostic_capture = false;
    std::uint64_t diagnostic_tick = 0;
    std::optional<std::string> diagnostic_snapshot;

    Slice<const BoardContactReport> contact_reports() const {
        return Slice<const BoardContactReport>(reports.data(), reports.size());
    }
    Slice<const RetailContactJacobian> solved_contacts() const {
        return Slice<const RetailContactJacobian>(contacts.data(), contacts.size());
    }
    /// The caller supplies current poses, processed steering targets, queued
    /// forces and narrow-phase contacts. It owns the separate force-queue reset
    /// in GeneralUpdate (82C02360); consuming forces does not clear the queue.
    void advance(std::array<BodySnapshot, BODY_COUNT>& bodies, BoardHook& hook, const BoardForceQueue& forces,
                 Slice<const BoardCollision> collisions, std::array<float, 2> truck_targets,
                 BoardStepSettings settings);

    void advance_attached(std::array<BodySnapshot, BODY_COUNT>& bodies, BoardHook& hook,
                          const BoardForceQueue& forces, Slice<const BoardCollision> collisions,
                          std::array<float, 2> truck_targets, BoardStepSettings settings, AttachedStep attached);
};

}  // namespace s3::physics::board_step
