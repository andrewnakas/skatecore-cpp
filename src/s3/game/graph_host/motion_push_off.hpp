// Port of skate-game/src/graph_host/motion_offboard/push_off.rs (module `motion_push_off`).
//! IsPushOffEnabled: original TU3 registration82F88150 -> ctor82BC62F8.
//! The ctor installs vtable8231F6D4; predicate slot+48 is8281DD70.
//! Original bytes386000014E800020 are li r3,1; blr. There is no physical
//! flag read, time gate or runtime setting in this leaf. The graph executor
//! remains responsible for the common active/mask/negation semantics.
#pragma once
#include <optional>
#include <string>

#include "s3/core/rs.hpp"
#include "s3/data/state_graph.hpp"

namespace s3::game::graph_host::motion_push_off {

using s3::data::state_graph_attributes::Attributes;

/// Unit struct `IsPushOffEnabled`.
struct IsPushOffEnabled {
    static std::optional<IsPushOffEnabled> parse(const Attributes& attributes) {
        const std::string* name = attributes.text("name");
        if (name != nullptr && *name == "IsPushOffEnabled") return IsPushOffEnabled{};
        return std::nullopt;
    }

    ///8281DD70: unconditional true in the verified original Skate3 TU3 image.
    bool evaluate() const { return true; }
};
S3_DERIVE_EQ_EMPTY(IsPushOffEnabled)

}  // namespace s3::game::graph_host::motion_push_off
