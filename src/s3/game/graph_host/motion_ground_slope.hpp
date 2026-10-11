// Port of skate-game/src/graph_host/motion_ground_slope.rs (module `motion_ground_slope`).
//! GroundSlopeType: TU3 ctor82BA7BD8 and condition82BA7E80.
#pragma once
#include <cstdint>
#include <string>

#include "s3/core/rs.hpp"
#include "s3/data/state_graph.hpp"

namespace s3::game::graph_host::motion_ground_slope {

using s3::data::state_graph_attributes::Attributes;

/// Rust tuple struct `GroundSlopeType(u32)`.
struct GroundSlopeType {
    std::uint32_t _0 = 0;

    static rs::Result<GroundSlopeType, std::string> parse(const Attributes& attributes) {
        const std::string* name = attributes.text("slopetype");
        if (name == nullptr) return rs::make_err(std::string("GroundSlopeType requires slopetype"));
        static const char* const names[] = {"Flat",          "StairUpShallow", "StairUpSteep",
                                            "StairDnShallow", "StairDnSteep",   "RampUpShallow",
                                            "RampUpSteep",    "RampDnShallow",  "RampDnSteep"};
        for (std::uint32_t value = 0; value < 9; ++value) {
            if (*name == names[value]) return GroundSlopeType{value};
        }
        // The native constructor has no fallback assignment. Reject malformed
        // host data rather than reproducing an uninitialized member read.
        return rs::make_err("Unknown GroundSlopeType slopetype " + *name);
    }

    /// Exactly PhysOut bundle+72 (OffBoard), integer field+88. The physical
    /// producer owns the classification; this graph leaf does not infer slope.
    bool matches(std::uint32_t physical_slope_type_88) const { return physical_slope_type_88 == _0; }
};
S3_DERIVE_EQ(GroundSlopeType, _0)

}  // namespace s3::game::graph_host::motion_ground_slope
