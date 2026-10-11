// Port of skate-core/src/physics/solver/joint.rs.
// TU3 joint pass 82AE2BC8..82AE2E44, reconstructed from direct disassembly.
// Finite compiled coefficients are required; exceptional SIMD arithmetic is
// not claimed to match hardware. The packed public boundary remains unchanged.
#pragma once
#include <cstdint>

#include "s3/core/physics/slice.hpp"
#include "s3/core/physics/solver/joint_data.hpp"

namespace s3::physics::solver::joint {

/// pub(super)
void solve(Slice<std::uint32_t> record, Slice<std::uint32_t> a, Slice<std::uint32_t> b);

}  // namespace s3::physics::solver::joint
