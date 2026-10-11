// Port of skate-game/src/graph_host/motion_landing.rs (module `motion_landing`).
//! Original TU3 landing graph configuration and persistent instance state.
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include "s3/core/rs.hpp"
#include "s3/data/state_graph.hpp"

namespace s3::game::graph_host::motion_landing {

using s3::data::state_graph_attributes::Attributes;

/// Rust `enum Operation`; each variant is a nested struct held in `value`.
struct Operation {
    struct IsAnticipating {};
    struct IsLanding {};
    struct IsManualing {};
    struct IsDoingTrick {};
    struct SetLandingData {};
    struct DisableTricks {
        float length = 0.0f;
    };
    struct ChooseRandomLanding {
        std::int32_t count = 0;
    };

    std::variant<IsAnticipating, IsLanding, IsManualing, IsDoingTrick, SetLandingData, DisableTricks,
                 ChooseRandomLanding>
        value;

    Operation(IsAnticipating v) : value(v) {}
    Operation(IsLanding v) : value(v) {}
    Operation(IsManualing v) : value(v) {}
    Operation(IsDoingTrick v) : value(v) {}
    Operation(SetLandingData v) : value(v) {}
    Operation(DisableTricks v) : value(v) {}
    Operation(ChooseRandomLanding v) : value(v) {}

    static std::optional<Operation> parse(const Attributes& a);
};
S3_DERIVE_EQ_EMPTY(Operation::IsAnticipating)
S3_DERIVE_EQ_EMPTY(Operation::IsLanding)
S3_DERIVE_EQ_EMPTY(Operation::IsManualing)
S3_DERIVE_EQ_EMPTY(Operation::IsDoingTrick)
S3_DERIVE_EQ_EMPTY(Operation::SetLandingData)
S3_DERIVE_EQ(Operation::DisableTricks, length)
S3_DERIVE_EQ(Operation::ChooseRandomLanding, count)
S3_DERIVE_EQ(Operation, value)

/// Original MotionGraph CA4 flags, distinct from physical balance mode.
struct Flags {
    //825953B0 clears bits30..27 and sets bit25.
    bool anticipating = false;
    bool landing = false;
    bool manualing = false;
    bool doing_trick = false;
    bool tricks_allowed = true;
};

/// Completed physical conditioner publication; no inferred landing classification.
struct Physical {
    float height = 0.0f;
    float spin = 0.0f;
    std::uint32_t kind = 0;
    float last_good_landing_velocity = 0.0f;
};

/// Rust `enum Condition`; each variant is a nested struct held in `value`.
struct Condition {
    struct HasLandingType {
        std::uint32_t _0 = 0;
    };
    struct HasTiltToLargeForPreland {};
    struct AllowedToTrick {};

    std::variant<HasLandingType, HasTiltToLargeForPreland, AllowedToTrick> value;

    Condition(HasLandingType v) : value(v) {}
    Condition(HasTiltToLargeForPreland v) : value(v) {}
    Condition(AllowedToTrick v) : value(v) {}

    static std::optional<Condition> parse(const Attributes& a);
    rs::Result<bool, std::string> evaluate(std::optional<Physical> physical, const Flags& flags,
                                           std::optional<bool> override_preland) const;
};
S3_DERIVE_EQ(Condition::HasLandingType, _0)
S3_DERIVE_EQ_EMPTY(Condition::HasTiltToLargeForPreland)
S3_DERIVE_EQ_EMPTY(Condition::AllowedToTrick)
S3_DERIVE_EQ(Condition, value)

/// Original instance scalar8 (height or countdown) and byte12 (timer complete).
/// (Fields are `pub(super)` in Rust.)
struct State {
    float value = 0.0f;
    bool complete = false;
};

}  // namespace s3::game::graph_host::motion_landing
