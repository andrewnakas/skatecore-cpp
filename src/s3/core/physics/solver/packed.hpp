// Port of skate-core/src/physics/solver/packed.rs.
// Native solver records with host indices replacing guest reaction pointers.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "s3/core/physics/slice.hpp"

namespace s3::physics::solver::packed {

template <std::size_t WORDS>
struct Constraint {
    std::array<std::uint32_t, WORDS> words{};
    std::size_t reaction_a = 0;
    std::size_t reaction_b = 0;
};

using Contact = Constraint<64>;
using Joint = Constraint<96>;
using Drive = Constraint<96>;
using Reaction = std::array<std::uint32_t, 16>;

/// Complete contact → joint → drive loops of HorusPipeline (82AE27D0).
/// Contact position corrections remain distinct from velocity-producing
/// reactions; all families see preceding writes during the same iteration.
void solve(Slice<Contact> contacts, Slice<Joint> joints, Slice<Drive> drives,
           Slice<Reaction> reactions, std::uint32_t iterations);

}  // namespace s3::physics::solver::packed
