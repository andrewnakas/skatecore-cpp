// Port of skate-core/src/player/lifecycle.rs.
// PARTIAL PORT (stage C): only items used by physics
// (SkateboardControllerFields, SkateboardControllerActions). Extend in place.
#pragma once
#include <cstdint>

#include "s3/core/rs.hpp"

namespace s3::player::lifecycle {

/// Fields at `PhysicalPlayer+1844`'s `SkateboardController`.
struct SkateboardControllerFields {
    /// Controller+444. Its wider meaning is unresolved; every mode change here
    /// resets it with a 32-bit zero.
    std::uint32_t word_444 = 0;
    /// Controller+448. Values used by this path are stopped=0, held=1, free=2.
    std::uint32_t state_448 = 0;
    /// Controller+452, identified by `StopController` as the system-on byte.
    bool system_on_452 = false;
};
S3_DERIVE_EQ(SkateboardControllerFields, word_444, state_448, system_on_452)

/// Board and drive operations called by the controller branch. Their complete
/// implementations are `HoldSkateboard` `0x82D75370` and
/// `LetGoOfSkateboard` `0x82D75440`; this lifecycle never replaces them.
/// (Rust trait -> abstract interface.)
struct SkateboardControllerActions {
    virtual ~SkateboardControllerActions() = default;
    virtual void hold_skateboard() = 0;
    virtual void let_go_of_skateboard() = 0;
};

}  // namespace s3::player::lifecycle
