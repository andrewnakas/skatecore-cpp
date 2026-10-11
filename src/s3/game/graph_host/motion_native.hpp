// Port of skate-game/src/graph_host/motion_native.rs (module `motion_native`).
//! Small original MotionGraph leaves used by the live riding ancestry.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/state_graph.hpp"
#include "s3/game/graph_host/motion_riding.hpp"
#include "s3/game/graph_host/motion_scoring_trick.hpp"

namespace s3::game::graph_host::motion_animation {
class MotionAnimation;
}

namespace s3::game::graph_host::motion_native {

using motion_animation::MotionAnimation;
using motion_riding::RidingState;
using s3::animation::output::attributes::AttributeName;
using s3::data::state_graph_attributes::Attributes;

struct Physical {
    /// PhysOutSystemReckoning16 and96, original StoreLandingData82BAFFF0.
    std::array<float, 4> centre_of_mass_velocity{};
    std::array<float, 4> system_up{};
    /// PhysOutGround32 (Reckoning752 Z), original slide helper82595930.
    std::array<float, 4> board_reckoning_z{};
    /// Complete PhysOutGround0/16/32/48 from Reckoning752, for82BB31B0.
    std::array<std::array<float, 4>, 4> board_reckoning{};
};

struct GesturePhysical {
    bool ground321 = false;
    bool state_offboard75 = false;
    ///Host player gesture preferences, consumed only when starting a gesture.
    std::optional<std::array<std::uint32_t, 4>> selections;
    bool suppress_up = false;
    bool force_brake_bypass = false;
};

/// Rust `enum Operation`; each variant is a nested struct held in `value`.
struct Operation {
    struct StoreLandingData {};
    struct EndShimmy {
        float _0 = 0.0f;
    };
    struct Score {
        std::uint32_t regular = 0;
        std::uint32_t mirrored = 0;
    };

    std::variant<StoreLandingData, EndShimmy, Score> value;

    Operation(StoreLandingData v) : value(v) {}
    Operation(EndShimmy v) : value(v) {}
    Operation(Score v) : value(v) {}

    static rs::Result<std::optional<Operation>, std::string> parse(const Attributes& a);
};
S3_DERIVE_EQ_EMPTY(Operation::StoreLandingData)
S3_DERIVE_EQ(Operation::EndShimmy, _0)
S3_DERIVE_EQ(Operation::Score, regular, mirrored)
S3_DERIVE_EQ(Operation, value)

/// Meaningful graph outputs are retained even though a scoring UI is outside
/// this game.82595D08 sets a name and ORs a bit in the graph's score packet.
struct ScorePacket {
    std::optional<std::pair<AttributeName, std::array<float, 2>>> handplant;
    /// ScoringGrabs 82BBEF60: selected authored name and tweak vector.
    std::optional<std::pair<AttributeName, std::array<float, 2>>> grab;
    motion_scoring_trick::Names trick_names;
    std::optional<std::uint32_t> name;
    std::uint32_t flags = 0;

    void set(std::uint32_t value);
};

///CreateInstance82BBAA98 seeds velocity0 and prior-air false. (Fields private in Rust.)
struct LandingData {
    float previous_velocity = 0.0f;
    bool was_air = false;

    void update(std::uint32_t category, Physical p, RidingState& riding);
};

void end_shimmy(MotionAnimation& animation, float seconds);

}  // namespace s3::game::graph_host::motion_native
