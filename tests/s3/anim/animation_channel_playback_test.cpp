// Port of the #[cfg(test)] module in skate-core/src/animation/channel_playback.rs.
#include "harness.hpp"
#include "s3/core/animation/channel_playback.hpp"

using namespace s3::animation::channel_playback;

namespace {
ChannelSettings settings() { return ChannelSettings{0, false, false, 1.0f, 0.5f, true, 0.5f, false, false}; }
}  // namespace

S3_TEST(preincrement_fade_and_expiry_order_preserve_native_first_frame) {
    auto channel = ChannelPlayback::new_(settings());
    CHECK(!channel.advance(0.25f, 2.0f, 0.0f));
    CHECK_EQ(channel.weight, 0.0f);
    CHECK(channel.advance(0.25f, 2.0f, 0.0f));
    CHECK_EQ(channel.weight, 0.5f);
    channel.end();
    CHECK(channel.advance(0.25f, 2.0f, 0.25f));
    CHECK_EQ(channel.weight, 1.0f);
    CHECK(!channel.expired());
    channel.advance(0.25f, 2.0f, 0.5f);
    CHECK_EQ(channel.weight, 0.5f);
    CHECK(channel.expired());
}

S3_TEST(returning_channel_restores_from_existing_fade_without_zero_restart) {
    auto s = settings();
    s.hold_during_blend_in = false;
    auto channel = ChannelPlayback::new_(s);
    channel.advance(0.5f, 2.0f, 0.0f);
    channel.end();
    channel.advance(0.25f, 2.0f, 0.5f);
    CHECK(!channel.can_transition(false));
    CHECK(channel.can_transition(true));
    channel.transition(s, true);
    channel.advance(0.1f, 2.0f, 0.0f);
    CHECK_EQ(channel.weight, 0.5f);
    CHECK(!channel.expired());
}
