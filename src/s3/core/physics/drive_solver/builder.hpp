// Port of skate-core/src/physics/drive_solver/builder.rs.
// TU3 drive Jacobian construction, separate from iterative solving.
#pragma once
#include "s3/core/physics/drive_solver.hpp"

namespace s3::physics::drive_solver::builder {

/// Scalar translation of TU3 `DriveJacobian::Build`.
///
/// RenderWare pairs frame A with internal `Drive::m_bodyA` at `Drive+0x10`
/// and frame B with `Drive::m_bodyB` at `Drive+0x14`. `Simulation::AddDrive`
/// stores its second body argument as internal A and its first as internal B.
/// The linear error is frame-B minus frame-A; the solver therefore applies
/// positive impulse to frame A and negative impulse to frame B.
RetailDriveRows build_drive_rows(RetailDriveBodyState frame_a_body, RetailDriveBodyState frame_b_body,
                                 RetailDriveFrames frames, RetailDriveDynamics dynamics,
                                 float time_step);

}  // namespace s3::physics::drive_solver::builder

// `pub use builder::build_drive_rows`.
namespace s3::physics::drive_solver {
using builder::build_drive_rows;
}  // namespace s3::physics::drive_solver
