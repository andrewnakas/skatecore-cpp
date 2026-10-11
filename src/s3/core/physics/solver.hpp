// Port of skate-core/src/physics/solver.rs.
// Shared TU3 constraint iteration (`0x82AE27D0`).
//
// Constraint families must exchange reactions on every pass. Solving each
// family to completion independently changes the physical result.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "s3/core/physics/contact_solver.hpp"
#include "s3/core/physics/drive_solver.hpp"
#include "s3/core/physics/joint_solver.hpp"
#include "s3/core/physics/rigid_body.hpp"
#include "s3/core/physics/slice.hpp"

namespace s3::physics::solver {

using contact_solver::RetailContactJacobian;
using drive_solver::RetailDriveRows;
using joint_solver::RetailJointJacobian;
using rigid_body::RetailReactionCorrections;

/// pub(crate)
void compile_contact(std::array<std::uint32_t, 64>& record, float time_step);

/// pub(crate)
void solve_joint_record(Slice<std::uint32_t> record, Slice<std::uint32_t> a, Slice<std::uint32_t> b);

/// Host indices replace the two guest reaction pointers stored in a Jacobian.
struct JointConstraint {
    RetailJointJacobian jacobian;
    std::size_t reaction_a = 0;
    std::size_t reaction_b = 0;
};

/// Runs the retail contact → joint → drive ordering. The caller clears the
/// reaction workspace and builds fresh rows once per simulation step.
void solve_constraints(Slice<RetailContactJacobian> contacts, Slice<JointConstraint> joints,
                       Slice<RetailDriveRows> drives, Slice<RetailReactionCorrections> reactions,
                       std::uint32_t iterations);

}  // namespace s3::physics::solver

#include "s3/core/physics/solver/contact_build.hpp"
#include "s3/core/physics/solver/packed.hpp"
