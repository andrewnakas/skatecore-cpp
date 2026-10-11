// Port of skate-core/src/physics/solver/packing.rs.
// Semantic builder outputs → native compiled-record layout.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/physics/solver.hpp"

namespace s3::physics::solver::packing {

packed::Contact contact(const RetailContactJacobian& c);
packed::Drive drive(const RetailDriveRows& d);
packed::Reaction reaction(RetailReactionCorrections r);
RetailReactionCorrections unpack_reaction(const packed::Reaction& words);
std::array<float, 3> read_xyz(const std::uint32_t* words, std::size_t i);

}  // namespace s3::physics::solver::packing
