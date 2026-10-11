// Port of skate-core/src/graph/tests/conditions.rs.
#include <limits>

#include "harness.hpp"
#include "s3/core/graph/conditions.hpp"

using namespace s3::graph::conditions;
using s3::graph::intents::IntentMap;

namespace {
NumericCondition numeric(Comparison comparison, float threshold) {
    return NumericCondition{comparison, threshold, false};
}
}  // namespace

S3_TEST(comparison_preserves_presence_and_unordered_branch_semantics) {
    const NumericCondition none = numeric(Comparison::None, 0.0f);
    const ActionCondition condition = ActionCondition::HasIntent{"Pushing", none};
    IntentMap intents = IntentMap::new_();
    const ConditionInputs input{};
    CHECK(!condition.evaluate(input, intents, std::nullopt, {}).unwrap());
    intents.insert("Pushing", 0.0f);
    CHECK(condition.evaluate(input, intents, std::nullopt, {}).unwrap());
    CHECK(!none.matches(0.0f));
    CHECK(numeric(Comparison::GreaterAbsolute, 0.5f).matches(-0.75f));
    const float nan = std::numeric_limits<float>::quiet_NaN();
    CHECK(numeric(Comparison::GreaterEqual, 1.0f).matches(nan));
    CHECK(numeric(Comparison::LessEqual, 1.0f).matches(nan));
    CHECK(!numeric(Comparison::Greater, 1.0f).matches(nan));
    CHECK(!numeric(Comparison::Less, 1.0f).matches(nan));
}

S3_TEST(speed_and_state_conditions_require_their_real_distinct_outputs) {
    ConditionInputs input{};
    const ActionCondition speed = ActionCondition::Speed{true, numeric(Comparison::Less, 0.0f)};
    CHECK(speed.evaluate(input, IntentMap::new_(), std::nullopt, {}).is_err());
    input.speeds = SpeedInputs{4.0f, -3.0f, 2.0f};
    CHECK(speed.evaluate(input, IntentMap::new_(), std::nullopt, {}).unwrap());
    const ActionCondition state = ActionCondition::CurrentState{"Riding", std::size_t(1)};
    const std::array<std::optional<std::size_t>, 4> parents = {std::nullopt, std::size_t(0), std::size_t(1),
                                                               std::size_t(0)};
    CHECK(state.evaluate(input, IntentMap::new_(), std::size_t(1), parents).unwrap());
    CHECK(state.evaluate(input, IntentMap::new_(), std::size_t(2), parents).unwrap());
    CHECK(!state.evaluate(input, IntentMap::new_(), std::size_t(3), parents).unwrap());
    CHECK(!state.evaluate(input, IntentMap::new_(), std::nullopt, parents).unwrap());
}

S3_TEST(push_brake_uses_ground_angle_and_skeleton_override) {
    const PushBrakeInputs ground{1.0f, false, 45.0f};
    CHECK(!ground.disabled());
    PushBrakeInputs a = ground;
    a.ground_axis_y = 0.0f;
    CHECK(a.disabled());
    PushBrakeInputs b = ground;
    b.skeleton_disables_push_brake = true;
    CHECK(b.disabled());
}
