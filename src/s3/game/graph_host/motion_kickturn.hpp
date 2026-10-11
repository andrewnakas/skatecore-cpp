// Port of skate-game/src/graph_host/motion_kickturn.rs (module `motion_kickturn`).
//! Original TU3 low-speed KickTurn behaviors, connected to MotionHost.
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include "s3/core/animation/kickturn.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/collections.hpp"
#include "s3/data/state_graph.hpp"

namespace s3::game::graph_host::motion_animation {
class MotionAnimation;
}

namespace s3::game::graph_host::motion_kickturn {

using motion_animation::MotionAnimation;
using s3::data::state_graph_attributes::Attributes;
namespace kickturn = s3::animation::kickturn;

/// Rust `enum Operation { ResetTimer, Steering(kickturn::Parameters) }`.
struct Operation {
    struct ResetTimer {};
    struct Steering {
        kickturn::Parameters _0;
    };

    std::variant<ResetTimer, Steering> value;

    Operation(ResetTimer v) : value(v) {}
    Operation(Steering v) : value(v) {}

    static std::optional<Operation> parse(const Attributes& a);
};
S3_DERIVE_EQ_EMPTY(Operation::ResetTimer)
S3_DERIVE_EQ(Operation::Steering, _0)
S3_DERIVE_EQ(Operation, value)

rs::Result<kickturn::Settings, std::string> load_settings(const s3::data::Collections& data);

rs::Result<rs::Unit, std::string> execute(kickturn::State& state, kickturn::Parameters parameters,
                                          const kickturn::Settings& settings, MotionAnimation& animation, float dt,
                                          std::uint8_t phase);

}  // namespace s3::game::graph_host::motion_kickturn
