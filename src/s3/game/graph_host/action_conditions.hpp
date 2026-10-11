// Port of skate-game/src/graph_host/action_conditions.rs.
//! Original ActionGraph leaves that consume the prior animation/state output.
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/graph/conditions.hpp"
#include "s3/core/graph/controller.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/state_graph.hpp"
#include "s3/game/input/gesture_catalog.hpp"
#include "s3/game/graph_host/motion_gameplay_conditions.hpp"

namespace s3::game::graph_host::action {
class ActionHost;
}

namespace s3::game::graph_host::action_conditions {

using motion_gameplay_conditions::GameplayCondition;
using s3::animation::output::attributes::AttributeName;
using s3::data::state_graph_attributes::Attributes;
using s3::graph::conditions::NumericCondition;

/// Rust `enum ActionCondition`; each variant is a nested struct held in `value`.
struct ActionCondition {
    struct Gesture {
        s3::game::input::gesture_catalog::Group _0 = s3::game::input::gesture_catalog::Group::Square;
    };
    struct AnimationAttribute {
        AttributeName name;
        std::int32_t sequence_id = 0;
        NumericCondition numeric;
    };
    struct ParentTime {
        std::optional<std::size_t> target;
        NumericCondition numeric;
    };
    struct Physical {
        GameplayCondition _0;
    };
    struct Tricking {};
    struct DroppingIn {};
    struct InLocomotion {};
    struct DisableDismount {};
    struct TimeToLand {
        NumericCondition _0;
    };

    std::variant<Gesture, AnimationAttribute, ParentTime, Physical, Tricking, DroppingIn, InLocomotion,
                 DisableDismount, TimeToLand>
        value;

    ActionCondition(Gesture v) : value(v) {}
    ActionCondition(AnimationAttribute v) : value(v) {}
    ActionCondition(ParentTime v) : value(v) {}
    ActionCondition(Physical v) : value(std::move(v)) {}
    ActionCondition(Tricking v) : value(v) {}
    ActionCondition(DroppingIn v) : value(v) {}
    ActionCondition(InLocomotion v) : value(v) {}
    ActionCondition(DisableDismount v) : value(v) {}
    ActionCondition(TimeToLand v) : value(v) {}

    static rs::Result<std::optional<ActionCondition>, std::string> parse(const Attributes& a);
    rs::Result<bool, std::string> evaluate(const action::ActionHost& host,
                                           const s3::graph::controller::Frame& frame) const;
};
S3_DERIVE_EQ(ActionCondition::Gesture, _0)
S3_DERIVE_EQ(ActionCondition::AnimationAttribute, name, sequence_id, numeric)
S3_DERIVE_EQ(ActionCondition::ParentTime, target, numeric)
S3_DERIVE_EQ(ActionCondition::Physical, _0)
S3_DERIVE_EQ_EMPTY(ActionCondition::Tricking)
S3_DERIVE_EQ_EMPTY(ActionCondition::DroppingIn)
S3_DERIVE_EQ_EMPTY(ActionCondition::InLocomotion)
S3_DERIVE_EQ_EMPTY(ActionCondition::DisableDismount)
S3_DERIVE_EQ(ActionCondition::TimeToLand, _0)
S3_DERIVE_EQ(ActionCondition, value)

}  // namespace s3::game::graph_host::action_conditions
