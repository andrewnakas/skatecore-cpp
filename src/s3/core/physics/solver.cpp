// Port of skate-core/src/physics/solver.rs.
#include "s3/core/physics/solver.hpp"

#include <vector>

#include "s3/core/physics/solver/joint.hpp"
#include "s3/core/physics/solver/packing.hpp"

namespace s3::physics::solver {

void compile_contact(std::array<std::uint32_t, 64>& record, float time_step) {
    contact_build::build(record, time_step);
}

void solve_joint_record(Slice<std::uint32_t> record, Slice<std::uint32_t> a, Slice<std::uint32_t> b) {
    joint::solve(record, a, b);
}

void solve_constraints(Slice<RetailContactJacobian> contacts, Slice<JointConstraint> joints,
                       Slice<RetailDriveRows> drives, Slice<RetailReactionCorrections> reactions,
                       std::uint32_t iterations) {
    std::vector<packed::Contact> c;
    for (const auto& contact : contacts) c.push_back(packing::contact(contact));
    std::vector<packed::Joint> j;
    for (const auto& joint : joints) j.push_back(packed::Joint{joint.jacobian.words, joint.reaction_a, joint.reaction_b});
    std::vector<packed::Drive> d;
    for (const auto& drive : drives) d.push_back(packing::drive(drive));
    std::vector<packed::Reaction> r;
    for (const auto& reaction : reactions) r.push_back(packing::reaction(reaction));
    packed::solve(c, j, d, r, iterations);
    for (std::size_t i = 0; i < contacts.size(); ++i) contacts[i].words = c[i].words;
    for (std::size_t i = 0; i < joints.size(); ++i) joints[i].jacobian.words = j[i].words;
    for (std::size_t i = 0; i < drives.size(); ++i) {
        drives[i].accumulated_linear_impulse = packing::read_xyz(d[i].words.data(), 2);
        drives[i].accumulated_angular_impulse = packing::read_xyz(d[i].words.data(), 3);
    }
    for (std::size_t i = 0; i < reactions.size(); ++i) reactions[i] = packing::unpack_reaction(r[i]);
}

}  // namespace s3::physics::solver
