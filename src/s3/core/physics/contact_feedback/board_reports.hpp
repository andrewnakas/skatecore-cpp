// Port of skate-core/src/physics/contact_feedback/board_reports.rs.
//
// Solver observations consumed by the board's post-physics update.
//
// TU3 82AE1608 observes completed contact rows after integration; 827682B0
// then constructs at most sixteen reports per board, in contact order.
// Rust owns the report storage and part identifiers. No guest pointers or
// copied native map/locking infrastructure are needed for this single board.
#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "s3/core/math.hpp"
#include "s3/core/physics/assembly.hpp"
#include "s3/core/physics/board.hpp"
#include "s3/core/physics/board_step/collision_body.hpp"
#include "s3/core/physics/contact_solver.hpp"
#include "s3/core/physics/slice.hpp"

namespace s3::physics::contact_feedback::board_reports {

using assembly::BodySnapshot;
using board::BODY_COUNT;
using board::BodyId;
using board_step::CollisionBody;
using contact_solver::RetailContactJacobian;
using math::Vector3;

struct BoardContactReport {
    BodyId part = BodyId::RightFrontWheel;
    CollisionBody other;
    bool is_body_a = false;
    /// Oriented toward this board, as ContactInfo+16.
    Vector3 normal;
    Vector3 position;
    /// Linear velocities after integration. Native does not add omega cross r.
    Vector3 relative_linear_velocity;
    std::uint16_t other_surface = 0;
    /// The underlying spy retains A's orientation even in a B-side report.
    Vector3 normal_force_on_a;
    Vector3 friction_force_on_a;
    std::array<Vector3, 2> tangents{};
};
S3_DERIVE_EQ(BoardContactReport, part, other, is_body_a, normal, position, relative_linear_velocity,
             other_surface, normal_force_on_a, friction_force_on_a, tangents)

/// pub(crate). Replace this frame's reports from the actual solved rows. All seven board
/// parts share one report budget and one owner; their self contacts are excluded
/// from reporting, independently of whether they participated in the solver.
void collect(std::vector<BoardContactReport>& output, Slice<const RetailContactJacobian> contacts,
             const std::array<BodySnapshot, BODY_COUNT>& bodies, float frequency);

}  // namespace s3::physics::contact_feedback::board_reports
