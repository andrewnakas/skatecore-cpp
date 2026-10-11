// Port of skate-core/src/scoring.rs (#[cfg(test)] mod tests, mod repetition_class_tests).
#include <cstdint>
#include <optional>

#include "harness.hpp"
#include "s3/core/scoring.hpp"

namespace {
using namespace s3::scoring;

const Scorable FLIP = Scorable{96, 3, 2};
std::optional<std::int8_t> some(int v) { return std::int8_t(v); }

S3_TEST(collector_bank_is_single_use_and_publication_retains_repetition) {
    ScoreHolder h;
    h.end_trick(FLIP, 20.0f);
    h.finish_collector();
    h.reward_sequence(1.1f);
    h.reward_sequence(1.1f);
    CHECK_EQ(h.snapshot.accumulated, 22.0f);
    h.publish(22.0f, true);
    CHECK_EQ(h.snapshot.line, 22.0f);
    CHECK(h.repetition_count(FLIP) == some(1));
    h.finish_line();
    CHECK_EQ(h.snapshot.completed_lines, 22.0f);
    CHECK(h.repetition_count(FLIP) == some(0));
}

S3_TEST(metric_preserves_flip_bucket_other_classes_flush_it) {
    ScoreHolder h;
    h.end_trick(FLIP, 20.0f);
    h.end_trick(Scorable{129, 5, 0}, 4.0f);
    CHECK_EQ(h.snapshot.general_pending, 4.0f);
    CHECK_EQ(h.snapshot.fingerflip_pending, 20.0f);
    h.end_trick(Scorable{128, 4, 1}, 2.0f);
    CHECK_EQ(h.snapshot.general_pending, 26.0f);
    CHECK_EQ(h.snapshot.fingerflip_pending, 0.0f);
}

S3_TEST(cancellation_after_collector_exit_prevents_pending_reward) {
    ScoreHolder h;
    h.end_trick(FLIP, 20.0f);
    h.finish_collector();
    h.cancel_pending();
    h.reward_sequence(1.1f);
    CHECK_EQ(h.snapshot.accumulated, 0.0f);
    h.set_suppressed(true);
    h.end_trick(FLIP, 20.0f);
    CHECK(h.repetition_count(FLIP) == some(1));
}

/// 82DA5D18 exempts class 5 and nothing else. Handplants are class 6 and must take
/// the repetition penalty like any other trick.
S3_TEST(only_the_metric_class_escapes_the_repetition_penalty) {
    auto scorable = [](std::uint32_t class_) { return Scorable{139, class_, 6}; };
    CHECK_MSG(!scorable(5).repetition_applies(), "class 5 is the exemption");
    for (std::uint32_t class_ : {0u, 1u, 2u, 3u, 4u, 6u, 7u, 11u, 12u}) {
        CHECK_MSG(scorable(class_).repetition_applies(), "class " << class_ << " must be penalised");
    }
    // An id outside the table is not scorable at all.
    CHECK(!(Scorable{SCORABLE_COUNT, 6, 6}.repetition_applies()));
}
}  // namespace
