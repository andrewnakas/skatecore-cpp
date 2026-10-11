// Port of skate-game/src/graph_host/motion_offboard/reset.rs (module `motion_reset`).
//! Original registered reset handlers:82F88440/82F89600.
//! Both have Begin effects; Update and End point to82B61BB8 (raw blr).
#pragma once
#include <optional>
#include <string>

#include "s3/core/rs.hpp"
#include "s3/data/state_graph.hpp"

namespace s3::game::graph_host::motion_reset {

using s3::data::state_graph_attributes::Attributes;

///Required integration boundary for the single ActionGraph, MotionGraph and
///playback owners. No default implementations or fabricated reset observations.
/// Rust `trait ResetOwners { type Error; ... }`: any type providing these four methods, each
/// returning `rs::Result<rs::Unit, Error>`, with a member alias `using Error = ...;`.
///   - clear_action_intents(): 82C0E308 ->82BC1B68: clear actual ActionGraph intent map only.
///   - reset_motion_graph_publications(): 825953B0: filtered intents, current MG attrs,
///     selective scalar writes, motion intents and output2664..2680. Preserve graph execution
///     and trick data.
///   - reset_skater_animation_playback(): 82B98050 +82D1CAE8 +82D180E8: masks/stance bit,
///     channel teardown, playback tree/transition/attribute/control reset on the canonical owner.
///   - reset_to_given_stance(): 82B97308 consumes saved15200 via82B972A8; no playback reset.

enum class Operation {
    SkaterAnimation,
    GivenStance,
};

/// `Operation::parse`.
inline std::optional<Operation> parse_operation(const Attributes& a) {
    const std::string* name = a.text("name");
    if (name == nullptr) return std::nullopt;
    if (*name == "ResetSkaterAnimation") return Operation::SkaterAnimation;
    if (*name == "ResetToGivenStance") return Operation::GivenStance;
    return std::nullopt;
}

/// `Operation::begin`.
///82BB8F70 guarantees AG-intents -> MG-output/intents -> SkaterAnim order.
///The service must borrow the actual owners; resetting AG/MG controllers or
///replacing all animation state with Default would change native behavior.
template <class S>
rs::Result<rs::Unit, typename S::Error> begin(Operation self, S& owners) {
    switch (self) {
        case Operation::SkaterAnimation: {
            auto cleared = owners.clear_action_intents();
            if (cleared.is_err()) return cleared;
            auto reset = owners.reset_motion_graph_publications();
            if (reset.is_err()) return reset;
            return owners.reset_skater_animation_playback();
        }
        case Operation::GivenStance:
            return owners.reset_to_given_stance();
    }
    return rs::Unit{};
}

}  // namespace s3::game::graph_host::motion_reset
