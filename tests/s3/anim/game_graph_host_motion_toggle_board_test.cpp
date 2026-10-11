// Port of skate-game/src/graph_host/motion_offboard/toggle_board/tests.rs.
#include <cmath>

#include "harness.hpp"
#include "s3/core/animation/playback_clip.hpp"
#include "s3/core/animation/playback_tree.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/game/graph_host/motion_toggle_board.hpp"

using namespace s3::game::graph_host::motion_toggle_board;
using s3::animation::channel_playback::ChannelSettings;
using s3::animation::playback_clip::PlaybackClip;
using s3::animation::playback_tree::PlaybackTree;
using s3::animation::skeleton_input::name::encode;
using s3::player::offboard::toggle_board::Phase;

namespace {
MotionAnimation animation() {
    auto metadata = s3::data::AnimationMetadata::parse(
        R"({"version":1,"source_bank":"test-only","source_sha256":"0000000000000000000000000000000000000000000000000000000000000000","source_bytes":48,"clips":[],"unsupported_trees":[]})");
    if (!metadata.ok) S3_PANIC(metadata.error.c_str());
    return MotionAnimation::from_metadata(metadata.value);
}
Physical physical() {
    Physical p;
    p.grabbing_object = false;
    p.holding_board = true;
    p.free_board = false;
    p.retrieval_blocked = false;
    p.retrieval_active = false;
    p.yaw_radians = 0.0f;
    p.pitch_radians = 0.0f;
    return p;
}
}  // namespace

S3_TEST(dropping_terminal_tick_publishes_physical_attribute_and_animation_parameter) {
    State state;
    state.phase = Phase::DropPlaying;
    MotionAnimation a = animation();
    execute(state, a, BoardControls{}, physical(), false, 1).unwrap();
    CHECK_EQ(a.motion_attributes.size(), std::size_t(1));
    CHECK(a.motion_attributes[0].name == encode("OB_DroppingBoard"));
    CHECK_EQ(a.motion_attributes[0].value, 1.0f);
    CHECK(!a.motion_intents.contains_key("OB_DroppingBoard"));
    CHECK(a.pending_parameter(encode("OB_DroppingBoard")) == std::optional<float>(1.0f));
    CHECK(state.phase == Phase::Idle);
}

S3_TEST(all_toggle_publications_reach_physics_attributes_and_animation_parameters) {
    MotionAnimation a = animation();
    for (const char* name : {"OB_RetrievingBoard", "OB_DroppingBoard", "OB_RetrieveBoard"}) {
        publish(a, name);
        const auto& packet = a.motion_attributes.back();
        CHECK(packet.name == encode(name));
        CHECK_EQ(packet.value, 1.0f);
        CHECK(a.pending_parameter(encode(name)) == std::optional<float>(1.0f));
        CHECK(!a.motion_intents.contains_key(name));
    }
    //Yaw/pitch parameterize the real retrieval tree; native does not append
    //these to the physical MG attribute list.
    attribute(a, "yaw", 45.0f);
    attribute(a, "pitch", -5.0f);
    CHECK_EQ(a.motion_attributes.size(), std::size_t(3));
    CHECK(a.pending_parameter(encode("yaw")) == std::optional<float>(45.0f));
    CHECK(a.pending_parameter(encode("pitch")) == std::optional<float>(-5.0f));
    a.begin_graph_update();
    CHECK_MSG(a.motion_attributes.empty(), "Recall pulse leaked to next frame");
}

S3_TEST(lifecycle_end_leaves_persistent_channel_and_begin_resets_state) {
    MotionAnimation a = animation();
    ChannelSettings settings;
    settings.priority = 0;
    settings.keep_alive = true;
    settings.mirrored = false;
    settings.speed = 1.0f;
    settings.blend_in = 0.0f;
    settings.blend_out = 0.0f;
    settings.hold_during_blend_in = false;
    settings.hold_during_blend_out = false;
    settings.use_attributes = false;
    a.channels.insert(std::string(CHANNEL),
                      PlaybackTree::Clip{"test", PlaybackClip::new_(301.0f, 30.0f, 1.0f, 0, {})}, settings);
    a.channels.advance(0.3f, 0.0f);
    CHECK(std::fabs(a.channels.elapsed(CHANNEL) - 0.3f) < 0.00001f);
    CHECK_EQ(a.channels.elapsed("missing"), 0.0f);
    State state;
    state.phase = Phase::DropPlaying;
    execute(state, a, BoardControls{}, std::nullopt, std::nullopt, 2).unwrap();
    CHECK(state.phase == Phase::DropPlaying);
    execute(state, a, BoardControls{}, std::nullopt, std::nullopt, 0).unwrap();
    CHECK(state.phase == Phase::Idle);
    a.channels.advance(1.0f, 0.0f);
    a.channels.retire();
    CHECK(a.channels.has(CHANNEL));
}
