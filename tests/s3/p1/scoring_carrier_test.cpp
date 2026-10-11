// Port of skate-core/src/scoring/carrier.rs (#[cfg(test)] mod tests).
#include "harness.hpp"
#include "s3/core/scoring/carrier.hpp"

namespace {
using namespace s3::scoring;
using namespace s3::scoring::carrier;

Carrier carrier(std::uint32_t start) {
    return Carrier::new_(Scorable{96, 3, 2}, 100, 0.2f, 2.25f, start, 4, false, false);
}

S3_TEST(announcement_clock_wraps_and_does_not_credit_twice) {
    Carrier c = carrier(UINT32_MAX - 2);
    CHECK(!c.announce(0, 0.3f));
    CHECK(c.announce(1, 0.3f));
    CHECK(!c.announce(2, 0.3f));
    CHECK(c.complete(0.3f));
    CHECK_EQ(c.reward, 20.0f);
    CHECK(!c.unannounced);
}

S3_TEST(early_completion_uses_authored_discount_without_announcement) {
    Carrier c = carrier(0);
    CHECK(c.complete(0.3f));
    CHECK_EQ(c.reward, 6.0f);
    CHECK(c.completed && c.unannounced && !c.announced);
}

S3_TEST(conversion_announces_replacement_without_completing_it) {
    Carrier previous = carrier(0);
    Carrier next = carrier(1);
    previous.convert_to(next, 0.3f);
    CHECK(previous.completed);
    CHECK_EQ(previous.reward, 0.0f);
    CHECK(next.announced && !next.completed);
    CHECK_EQ(next.reward, 20.0f);
    CHECK(!next.announce(5, 0.3f));
}
}  // namespace
