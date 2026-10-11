// Port of skate-core/src/physics/solver/packed.rs.
#include "s3/core/physics/solver/packed.hpp"

#include "s3/core/physics/solver/contact.hpp"
#include "s3/core/physics/solver/drive.hpp"
#include "s3/core/physics/solver/joint.hpp"

namespace s3::physics::solver::packed {

namespace {

using Kernel = void (*)(Slice<std::uint32_t>, Slice<std::uint32_t>, Slice<std::uint32_t>);

template <std::size_t N>
void validate(Slice<Constraint<N>> constraints, std::size_t count) {
    for (const auto& c : constraints) {
        S3_ASSERT(c.reaction_a != c.reaction_b);
        S3_ASSERT(c.reaction_a < count && c.reaction_b < count);
    }
}

template <std::size_t N>
void pass(Slice<Constraint<N>> constraints, Slice<Reaction> reactions, Kernel kernel) {
    for (auto& c : constraints) {
        Reaction& a = reactions[c.reaction_a];
        Reaction& b = reactions[c.reaction_b];
        kernel(Slice<std::uint32_t>(c.words), Slice<std::uint32_t>(a), Slice<std::uint32_t>(b));
    }
}

}  // namespace

void solve(Slice<Contact> contacts, Slice<Joint> joints, Slice<Drive> drives,
           Slice<Reaction> reactions, std::uint32_t iterations) {
    validate(contacts, reactions.size());
    validate(joints, reactions.size());
    validate(drives, reactions.size());
    for (std::uint32_t i = 0; i < iterations; ++i) {
        pass(contacts, reactions, contact::solve);
        pass(joints, reactions, joint::solve);
        pass(drives, reactions, drive::solve);
    }
}

}  // namespace s3::physics::solver::packed
