// Port of skate-game/src/graph_host/motion_scoring_trick.rs (module `motion_scoring_trick`).
//! ScoringTrick lifecycle from original Skate 3 TU3 82BCC030/82BBFA88.
//! Update publishes the same authored encoded name to both score-name slots;
//! Begin and End are empty. No animation, force, or trick-selection side effect.
#pragma once
#include <cstdint>
#include <optional>
#include <string>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/state_graph.hpp"

namespace s3::game::graph_host::motion_scoring_trick {

using s3::animation::output::attributes::AttributeName;
using s3::data::state_graph_attributes::Attributes;

/// MotionGraph full-object5924 and5948, retained alongside score flags5972.
/// Native24-byte slots contain five encoded words plus initialized padding;
/// host representation preserves the names without copying native padding.
struct Names {
    std::optional<AttributeName> first;
    std::optional<AttributeName> second;
};

struct Operation {
    AttributeName trick;

    static rs::Result<Operation, std::string> parse(const Attributes& attributes) {
        const std::string* trick = attributes.text("trick");
        if (trick == nullptr) return rs::make_err(std::string("ScoringTrick requires authored trick"));
        return Operation{s3::animation::skeleton_input::name::encode(*trick)};
    }

    /// Vtable823214D0: Update at+52, while Begin+48 and End+56 are blr.
    void execute(Names& names, std::uint32_t& flags, std::uint8_t phase) const {
        if (phase != 1) return;
        //8258FA20: oris flags,0x100; copies the two input names verbatim.
        //82BBFA88 supplies two copies of the same name: no mirror selection.
        flags |= 0x01000000u;
        names.first = trick;
        names.second = trick;
    }
};
S3_DERIVE_EQ(Operation, trick)

}  // namespace s3::game::graph_host::motion_scoring_trick
