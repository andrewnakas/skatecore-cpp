// Port of skate-game/src/graph_host/motion_offboard/dismount.rs (module `motion_dismount`).
//! DisableDismount condition82BA5368 in the original Skate3 TU3 executable.
#pragma once
#include <optional>

#include "s3/core/graph/conditions.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/state_graph.hpp"

namespace s3::game::graph_host::motion_dismount {

using s3::data::state_graph_attributes::Attributes;
using s3::graph::conditions::PushBrakeInputs;

/// Unit struct `Condition`.
struct Condition {
    static std::optional<Condition> parse(const Attributes& attributes) {
        const std::string* name = attributes.text("name");
        if (name != nullptr && *name == "DisableDismount") return Condition{};
        return std::nullopt;
    }

    ///Uses the same published Ground80.Y and Skeleton598 as DisablePushBrake.
    ///The native leaf requires the physical component; absence is an error.
    rs::Result<bool, const char*> evaluate(const PushBrakeInputs* physical) const {
        if (physical == nullptr) return rs::make_err("DisableDismount requires actual Ground80 and Skeleton598");
        //822F8C9C; strict SIMD comparison is false for an unordered normal.
        return rs::from_bits(0x3F23D70Au) > physical->ground_axis_y || physical->skeleton_disables_push_brake;
    }
};
S3_DERIVE_EQ_EMPTY(Condition)

}  // namespace s3::game::graph_host::motion_dismount
