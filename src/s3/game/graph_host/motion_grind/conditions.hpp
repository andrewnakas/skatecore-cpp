// Port of skate-game/src/graph_host/motion_grind/conditions.rs (module `motion_grind::conditions`).
//! Original S3 graph predicates. Producers and shared dispatch are external.
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include "s3/core/rs.hpp"
#include "s3/data/state_graph.hpp"

namespace s3::game::graph_host::motion_grind::conditions {

/// Grinds bundle16 except filtered bundle64 byte80 / Air bundle8 fields.
/// No defaults or inference from contacts, physical state ID, grabs or airborne.
struct Physical {
    bool filtered_grinding_80 = false;
    std::uint32_t blunting_136 = 0;
    std::uint32_t approach_268 = 0;
    std::uint32_t trick_out_240 = 0;
    bool air_grind_443 = false;
    float air_time_184 = 0.0f;
    bool dropping_in_324 = false;
};

/// Rust `enum Condition`; each variant is a nested struct held in `value`.
struct Condition {
    struct BluntingBackslash {};
    struct Approach {
        bool forwards = false;
    };
    struct TrickOutType {
        std::uint32_t value = 0;
    };
    struct LandingIntoGrind {};
    struct DroppingIn {};

    std::variant<BluntingBackslash, Approach, TrickOutType, LandingIntoGrind, DroppingIn> value;

    Condition(BluntingBackslash v) : value(v) {}
    Condition(Approach v) : value(v) {}
    Condition(TrickOutType v) : value(v) {}
    Condition(LandingIntoGrind v) : value(v) {}
    Condition(DroppingIn v) : value(v) {}

    static rs::Result<std::optional<Condition>, std::string> parse(
        const s3::data::state_graph_attributes::Attributes& a);
    bool evaluate(const Physical& p) const;
};
S3_DERIVE_EQ_EMPTY(Condition::BluntingBackslash)
S3_DERIVE_EQ(Condition::Approach, forwards)
S3_DERIVE_EQ(Condition::TrickOutType, value)
S3_DERIVE_EQ_EMPTY(Condition::LandingIntoGrind)
S3_DERIVE_EQ_EMPTY(Condition::DroppingIn)
S3_DERIVE_EQ(Condition, value)

}  // namespace s3::game::graph_host::motion_grind::conditions
