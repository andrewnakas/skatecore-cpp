// Port of skate-core/src/physics/joint_solver.rs.
#include "s3/core/physics/joint_solver.hpp"

#include "s3/core/physics/solver.hpp"

namespace s3::physics::joint_solver {

void solve_joint_iteration(RetailJointJacobian& jacobian, RetailJointReactionBlock& reaction_a,
                           RetailJointReactionBlock& reaction_b) {
    solver::solve_joint_record(jacobian.words, reaction_a.words, reaction_b.words);
}

}  // namespace s3::physics::joint_solver
