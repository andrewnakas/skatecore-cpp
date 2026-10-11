// Port of skate-game/src/graph_host/motion_hand_services/tests.rs.
#include "harness.hpp"
#include "s3/core/animation/playback_clip.hpp"
#include "s3/core/animation/playback_tree.hpp"
#include "s3/game/graph_host/motion_animation.hpp"
#include "s3/game/graph_host/motion_hand_services.hpp"

using namespace s3::game::graph_host::motion_hand_services;

namespace {
using Busy = std::array<std::uint32_t, 2>;
}

S3_TEST(nested_hand_lifetimes_preserve_other_owners) {
    HandServices state;
    const Operation bs = Operation::HandBusy{Hand::Backside};
    const Operation fs = Operation::HandBusy{Hand::Frontside};
    for (const Operation& operation : {bs, fs, bs}) state.apply(operation, 0);
    state.apply(bs, 1);
    CHECK(state.busy_hands == (Busy{2, 1}));
    state.apply(bs, 2);
    CHECK(state.busy_hands == (Busy{1, 1}));
    state.apply(fs, 2);
    CHECK(state.busy_hands == (Busy{1, 0}));
    state.apply(bs, 2);
    CHECK(state.busy_hands == (Busy{0, 0}));
}

S3_TEST(retail_counter_stores_wrap_without_saturating) {
    HandServices state;
    const Operation bs = Operation::HandBusy{Hand::Backside};
    state.apply(bs, 2);
    CHECK(state.busy_hands == (Busy{UINT32_MAX, 0}));
    state.apply(bs, 0);
    CHECK(state.busy_hands == (Busy{0, 0}));
}

S3_TEST(maintain_shove_is_an_update_latch_not_a_reference_count) {
    HandServices state;
    const Operation operation = Operation::MaintainShove{};
    CHECK(!state.keep_shove_channels);
    CHECK(!state.apply(operation, 0));
    CHECK(!state.keep_shove_channels);
    for (int i = 0; i < 2; ++i) CHECK(!state.apply(operation, 1));
    CHECK(state.keep_shove_channels);
    CHECK(state.apply(operation, 2));
    CHECK(!state.keep_shove_channels);
    CHECK(!state.apply(operation, 1));
    CHECK(state.keep_shove_channels);
}

S3_TEST(hand_selection_uses_exact_stock_comparison) {
    CHECK(hand_from_name("bs") == Hand::Backside);
    for (const char* name : {"fs", "BS", "", "both"}) CHECK(hand_from_name(name) == Hand::Frontside);
}

S3_TEST(maintain_end_fades_only_antic_through_existing_animation_runtime) {
    using s3::animation::channel_playback::ChannelSettings;
    using s3::animation::playback_clip::PlaybackClip;
    using s3::animation::playback_tree::PlaybackTree;
    using s3::game::graph_host::motion_animation::MotionAnimation;
    auto metadata = s3::data::AnimationMetadata::parse(
        R"({"version":1,"source_bank":"test-only","source_sha256":"0000000000000000000000000000000000000000000000000000000000000000","source_bytes":48,"clips":[],"unsupported_trees":[]})");
    if (!metadata.ok) s3test::fail(__FILE__, __LINE__, metadata.error);
    MotionAnimation animation = MotionAnimation::from_metadata(metadata.value);
    ChannelSettings settings;
    settings.priority = 0;
    settings.keep_alive = true;
    settings.mirrored = false;
    settings.speed = 1.0f;
    settings.blend_in = 0.0f;
    settings.hold_during_blend_in = false;
    settings.blend_out = 0.5f;
    settings.hold_during_blend_out = false;
    settings.use_attributes = false;
    for (const char* name : {"SkitchAntic", "Shove"}) {
        animation.channels.insert(name, PlaybackTree::Clip{name, PlaybackClip::new_(301.0f, 30.0f, 1.0f, 0, {})},
                                  settings);
    }
    HandServices state;
    state.execute(Operation::MaintainShove{}, 1, animation);
    state.execute(Operation::MaintainShove{}, 2, animation);
    CHECK(!state.keep_shove_channels);
    CHECK(animation.channels.has("SkitchAntic"));
    animation.channels.advance(0.25f, 0.0f);
    animation.channels.retire();
    CHECK(animation.channels.has("SkitchAntic"));
    animation.channels.advance(0.25f, 0.0f);
    animation.channels.retire();
    CHECK(!animation.channels.has("SkitchAntic"));
    CHECK(animation.channels.has("Shove"));
    // Repeating End with no antic must leave unrelated live channels intact.
    state.execute(Operation::MaintainShove{}, 2, animation);
    CHECK(animation.channels.has("Shove"));
}
