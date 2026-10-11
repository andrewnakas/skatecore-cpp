// Port of skate-core/src/scoring/session.rs (#[cfg(test)] mod tests).
#include "harness.hpp"
#include "s3/core/scoring/session.hpp"

namespace {
using namespace s3::scoring;
using namespace s3::scoring::session;

S3_TEST(newly_crossed_threshold_applies_to_the_following_publication) {
    const Rules rules{801.0f, {{{50.0f, 1.5f}, {450.0f, 2.0f}, {800.0f, 3.0f}}}, 799.0f, 400.0f, 0.0f};
    Session session;
    const Scorable trick{96, 3, 2};
    session.holder.end_trick(trick, 50.0f);
    session.holder.finish_collector();
    CHECK_EQ(session.publish_sequence(rules, 1.0f, false, true), 50.0f);
    CHECK_EQ(session.combo.multiplier, 1.5f);
    session.holder.end_trick(trick, 10.0f);
    session.holder.finish_collector();
    CHECK_EQ(session.publish_sequence(rules, 1.0f, false, true), 15.0f);
    CHECK_EQ(session.holder.snapshot.line, 65.0f);
    session.line.points = 0.0f;
    session.settle_line(false, true);
    CHECK(session.holder.repetition_count(trick) == std::optional<std::int8_t>(2));
    session.settle_line(false, false);
    CHECK(session.holder.repetition_count(trick) == std::optional<std::int8_t>(0));
}
}  // namespace
