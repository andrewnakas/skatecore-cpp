// Port of skate-core/src/input/steering_intentions.rs (#[cfg(test)] mod tests).
#include "harness.hpp"
#include "s3/core/input/steering_intentions.hpp"

namespace {
using namespace s3::input::steering_intentions;

S3_TEST(left_steering_preserves_signed_turn_and_hard_turn_symmetry) {
    const auto left = produce({-0.75f, 0.0f}, 0);
    const auto right = produce({0.75f, 0.0f}, 0);

    CHECK(left.turn == std::optional<float>(-0.75f));
    CHECK(right.turn == std::optional<float>(0.75f));
    const std::optional<float> negated = right.hard_turn ? std::optional<float>(-*right.hard_turn) : std::nullopt;
    CHECK(left.hard_turn == negated);
    CHECK(left.hard_turn_crouch == right.hard_turn_crouch);
}

S3_TEST(steering_gate_suppresses_all_three_emissions_without_changing_input) {
    const auto gated = produce({-0.75f, 0.0f}, 1);
    CHECK(gated == (SteeringIntentions{std::nullopt, std::nullopt, std::nullopt}));
}
}  // namespace
