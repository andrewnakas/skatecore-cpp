// Port of skate-core/src/player/conditioner_capabilities.rs (whole file).
// Conditioner capability publication from original Skate 3 TU3 0x8275F430.
//
// World writes ExternalInput+2780, then 0x82DA7888 copies that word to
// PhysOutScoring2+204. The word gates controller and camera behavior; it is
// not a count or mask of currently broken bones.
#pragma once
#include <cstdint>

#include "s3/core/rs.hpp"

namespace s3::player::conditioner_capabilities {

/// Host settings needed by the original whole-word capability calculation.
/// There is deliberately no implicit default for these gameplay choices.
struct ConditionerCapabilityContext {
    /// Original FEBackEnd state equals 2 (in front end). This is a menu gate.
    bool in_front_end;
    /// Original game settings +184 equals 1. UI option 29 is Hall of Meat mode.
    bool hall_of_meat_enabled;
    /// Original challenge subsystem +328 virtual method at vtable+12 is nonzero.
    /// Its precise challenge interface name remains unidentified. A host with
    /// no challenge system supplies false rather than inventing that system.
    bool challenge_query_active;
    /// Original challenge subsystem +512 points to a record whose byte 44 is
    /// nonzero. Only consulted when the challenge query above is active.
    bool challenge_configuration_enabled;

    /// Preserve all bits and branch priority from 0x8275F4D8..0x8275F558.
    std::uint32_t capabilities() const {
        if (in_front_end) {
            return 0;
        } else if (challenge_query_active && challenge_configuration_enabled) {
            return 0xfffffff7u;
        } else if (hall_of_meat_enabled && !challenge_query_active) {
            return 0xfffff7f7u;
        } else {
            return 0x000001c0u;
        }
    }
};
S3_DERIVE_EQ(ConditionerCapabilityContext, in_front_end, hall_of_meat_enabled, challenge_query_active,
             challenge_configuration_enabled)

}  // namespace s3::player::conditioner_capabilities
