// Port of skate-core/src/graph/tests/intent_handlers.rs.
#include "harness.hpp"
#include "s3/core/graph/intent_handlers.hpp"

using namespace s3::graph::intent_handlers;
using s3::input::graph_intents::IntentMutation;

S3_TEST(constant_keeps_begin_value_for_one_frame_then_removes) {
    auto intent = ConstMgIntent::new_(2.5f, false);
    CHECK_EQ(intent.begin(), IntentMutation(IntentMutation::Set{2.5f}));
    CHECK_EQ(intent.update(), IntentMutation(IntentMutation::None{}));
    CHECK_EQ(intent.update(), IntentMutation(IntentMutation::Remove{}));
}

S3_TEST(constant_on_update_does_not_republish) {
    auto intent = ConstMgIntent::new_(-1.0f, true);
    CHECK_EQ(intent.begin(), IntentMutation(IntentMutation::Set{-1.0f}));
    CHECK_EQ(intent.update(), IntentMutation(IntentMutation::None{}));
    CHECK_EQ(intent.update(), IntentMutation(IntentMutation::None{}));
}

S3_TEST(constant_end_removes_without_changing_instance_state) {
    auto intent = ConstMgIntent::new_(2.5f, false);
    intent.begin();
    const auto previous = intent;
    CHECK_EQ(intent.end(), IntentMutation(IntentMutation::Remove{}));
    CHECK_EQ(intent, previous);
}

S3_TEST(time_uses_presence_and_accumulates_instance_clock) {
    TimeMgIntent intent{};
    CHECK_EQ(intent.update(0.0f, 0.25f), IntentMutation(IntentMutation::Set{0.25f}));
    CHECK_EQ(intent.update(99.0f, 0.5f), IntentMutation(IntentMutation::Set{0.75f}));
    CHECK_EQ(intent.update(std::nullopt, 0.5f), IntentMutation(IntentMutation::Remove{}));
    CHECK_EQ(intent.elapsed, 0.0f);
}

S3_TEST(time_end_removes_without_resetting_instance_clock) {
    TimeMgIntent intent{};
    CHECK_EQ(intent.update(0.0f, 0.25f), IntentMutation(IntentMutation::Set{0.25f}));
    CHECK_EQ(intent.end(), IntentMutation(IntentMutation::Remove{}));
    CHECK_EQ(intent.elapsed, 0.25f);
}
