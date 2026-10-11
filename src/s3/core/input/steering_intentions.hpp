// Port of skate-core/src/input/steering_intentions.rs.
// Complete local Turn/HardTurn/HardTurnCrouch emission dataflow in 825999F0.
// Other intention outputs and the subsequent action/motion graphs are separate.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/rs.hpp"

namespace s3::input::steering_intentions {

struct SteeringIntentions {
    /// Absence is retained: native does not emit a zero Turn intention.
    std::optional<float> turn;
    std::optional<float> hard_turn;
    std::optional<float> hard_turn_crouch;
};
S3_DERIVE_EQ(SteeringIntentions, turn, hard_turn, hard_turn_crouch)

/// Consumes the current record after 825992D8 and actor+1908 captured at
/// 82599A38. No history field affects these three local emission sites.
SteeringIntentions produce(std::array<float, 2> left, std::uint32_t actor_flags_1908);

}  // namespace s3::input::steering_intentions
