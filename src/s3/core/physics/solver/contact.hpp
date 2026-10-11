// Port of skate-core/src/physics/solver/contact.rs.
// Contact iteration, TU3 82AE2914..82AE2B8C.
//
// The compiled row boundary is decoded once. Calculations preserve the source
// operation order for finite inputs; console exceptional-float behavior has
// not been established by hardware captures.
#pragma once
#include <cstdint>

#include "s3/core/physics/slice.hpp"
#include "s3/core/physics/solver/contact_data.hpp"

namespace s3::physics::solver::contact {

/// pub(super)
void solve(Slice<std::uint32_t> record, Slice<std::uint32_t> a, Slice<std::uint32_t> b);

}  // namespace s3::physics::solver::contact
