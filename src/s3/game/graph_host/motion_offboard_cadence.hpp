// Port of skate-game/src/graph_host/motion_offboard/cadence.rs (module `motion_offboard_cadence`).
//! BipedCadence82BB9E78, MatchCadence82BBA938/82BBAA00 and LocoState82BA7B50.
//! TU3 default.patched.xex SHA256
//!431b8eba23565affdc10d137df19b06fe286244cefb3e1a13f32693e9600395a.
#pragma once
#include <cstdint>
#include <optional>
#include <string>

#include "s3/core/animation/playback_parameters.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/state_graph.hpp"

namespace s3::game::graph_host::motion_offboard_cadence {

using s3::animation::playback_parameters::AttributeSink;
using s3::data::state_graph_attributes::Attributes;

enum class Operation {
    BipedCadence,
    MatchCadence,
};

/// `Operation::parse`. Constructors have no operation-specific XML configuration. The common
/// graph wrapper retains mask, active and ordering behavior.
std::optional<Operation> parse_operation(const Attributes& a);

///82BB9E78 calls SkaterAnim virtual40=82531200, a direct phase14652 store.
/// Supply the actual OffBoard80 and the existing AnimationState.phase owner.
/// Component absence skips the write. Begin/End are82B61BB8 no-ops.
/// (`animation_phase`: nullptr = None.)
void biped_cadence(std::optional<float> physical_phase, float* animation_phase, std::uint8_t phase);

/// Per-node state from82BBAA98: value8=0 and byte12=false. This is a captured
/// Begin value, not a second physical cadence or animation phase owner.
struct MatchCadence {
    float captured_phase = 0.0f;
    bool pending = false;

    ///82BBA938 requires both physical and animation components. Missing either
    /// leaves the previous capture and byte untouched.
    void begin(std::optional<float> physical_phase, bool animation_present);
    ///82BBAA00 publishes on every Update, even if byte12 is already false.
    /// It clears that byte only after writing through virtual80/SetAttribute.
    void update(AttributeSink* animation);
    ///Original vtable End82B61BB8 does not reset the capture or pending byte.
    void end() {}
};
S3_DERIVE_EQ(MatchCadence, captured_phase, pending)

struct LocoState {
    std::uint32_t expected = 0;  // private in Rust (constructed directly by its tests)

    ///82BA79E8 compares case-sensitive locostate values. An unknown spelling
    /// leaves native field28 unwritten; reject invalid data rather than invent0.
    static rs::Result<LocoState, std::string> parse(const Attributes& a);
    ///82BA7B50 requires the physical component and compares OffBoard84 exactly.
    /// The graph wrapper applies its separately parsed mask and negation.
    bool evaluate(std::uint32_t locomotion_state_84) const { return locomotion_state_84 == expected; }
};
S3_DERIVE_EQ(LocoState, expected)

}  // namespace s3::game::graph_host::motion_offboard_cadence
