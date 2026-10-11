// Port of skate-core/src/physics/contact_solver.rs.
#include "s3/core/physics/contact_solver.hpp"

#include "s3/core/physics/contact_layout.hpp"
#include "s3/core/physics/solver.hpp"

namespace s3::physics::contact_solver {

RetailContactJacobian build_contact_jacobian(contact::RetailContact contact, float time_step) {
    S3_ASSERT(rs::is_finite(time_step) && time_step > 0.0f);
    auto words = contact_layout::encode(contact);
    solver::compile_contact(words, time_step);
    return RetailContactJacobian{
        words,
        static_cast<std::size_t>(contact.body_a_workspace.reaction_id),
        static_cast<std::size_t>(contact.body_b_workspace.reaction_id),
    };
}

void solve_contact_jacobians(Slice<RetailContactJacobian> contacts,
                             Slice<rigid_body::RetailReactionCorrections> reactions,
                             std::uint32_t maximum_iterations) {
    solver::solve_constraints(contacts, {}, {}, reactions, maximum_iterations);
}

void solve_contact_jacobian_iteration(Slice<RetailContactJacobian> contacts,
                                      Slice<rigid_body::RetailReactionCorrections> reactions) {
    solve_contact_jacobians(contacts, reactions, 1);
}

}  // namespace s3::physics::contact_solver
