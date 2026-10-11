// Port of skate-core/src/physics/solver/drive.rs.
// TU3 drive pass 82AE2E78..82AE30D8, reconstructed from direct disassembly.
// Finite compiled coefficients are required; exceptional SIMD arithmetic is
// not claimed to match hardware. Linear/angular softness lanes are retained.
#pragma once
#include <cstdint>

#include "s3/core/physics/slice.hpp"

namespace s3::physics::solver::drive {

/// pub(super)
void solve(Slice<std::uint32_t> record, Slice<std::uint32_t> a, Slice<std::uint32_t> b);

}  // namespace s3::physics::solver::drive
