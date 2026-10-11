// Port of the `RawVector` / `RawMatrix` aliases from skate-core/src/player/input_phase/types.rs.
// Split into its own header only to break the Rust module cycle (types <-> air_output,
// pose_output, grind_*, teleport_state); re-exported by types.hpp and input_phase.hpp.
#pragma once
#include <array>
#include <cstdint>

namespace s3::player::input_phase::types {
using RawVector = std::array<std::uint32_t, 4>;
using RawMatrix = std::array<std::array<std::uint32_t, 4>, 4>;
}  // namespace s3::player::input_phase::types
