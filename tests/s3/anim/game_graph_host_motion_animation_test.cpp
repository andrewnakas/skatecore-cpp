// Port of skate-game/src/graph_host/tests/animation_jump_into.rs
// (`#[cfg(test)] #[path = "tests/animation_jump_into.rs"] mod jump_into_tests` of motion_animation).
#include "harness.hpp"
#include "s3/core/animation/phase_blend.hpp"
#include "s3/core/animation/playback_clip.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/game/graph_host/motion_animation.hpp"

using namespace s3::game::graph_host::motion_animation;
using s3::animation::phase_blend::PhaseBlend;
using s3::animation::playback_clip::ClipAttribute;
using s3::animation::playback_clip::PlaybackClip;
using s3::animation::skeleton_input::name::encode;

namespace {

MotionAnimation owner(PlaybackTree tree) {
    // Synthetic independent fixture; no claim that this is stock animation data.
    auto metadata = AnimationMetadata::parse(R"({
        "version":1,"source_bank":"synthetic-test",
        "source_sha256":"0000000000000000000000000000000000000000000000000000000000000000",
        "source_bytes":48,"clips":[],"unsupported_trees":[]
    })");
    if (!metadata.ok) s3test::fail(__FILE__, __LINE__, metadata.error);
    MotionAnimation animation = MotionAnimation::from_metadata(std::move(metadata.value));
    animation.current = std::move(tree);
    return animation;
}

PlaybackTree clip(float begin, float blend_value) {
    return PlaybackTree::Clip{
        "synthetic-manual-takeoff",
        PlaybackClip::new_(61.0f, 60.0f, 1.0f, 0,
                           {
                               ClipAttribute{encode("manualInto"), 0, begin, begin + 0.1f, {s3::rs::to_bits(17.0f)}},
                               ClipAttribute{encode("height"), 0, -1.0f, -1.0f, {s3::rs::to_bits(blend_value)}},
                           })};
}

}  // namespace

S3_TEST(jump_into_queries_inactive_marker_begin_and_resets_clip_history) {
    MotionAnimation animation = owner(clip(0.625f, 0.0f));
    const auto name = encode("manualInto");
    animation.refresh_tree_attributes().unwrap();
    bool any = false;
    for (const auto& a : animation.tree_attributes()) any = any || a.name == name;
    CHECK(!any);
    animation.jump_into(name).unwrap();
    CHECK_EQ(animation.current_time().unwrap(), 0.625f);
    const auto* c = std::get_if<PlaybackTree::Clip>(&animation.current->value);
    if (!c) S3_PANIC("explicit panic");
    CHECK_EQ(c->clip.clock.previous_time, 0.625f);
    CHECK_EQ(c->clip.clock.loops_since_evaluation, 0u);
    CHECK(animation.motion_attributes.empty());
    CHECK(animation.settable.entries().empty());
}

S3_TEST(jump_into_applies_pending_blend_before_reading_marker_time) {
    PlaybackTree tree = PlaybackTree::PhaseBlend{PhaseBlend::new_(encode("height"), [] {
                                                     std::vector<PlaybackTree> v;
                                                     v.push_back(clip(0.25f, 0.0f));
                                                     v.push_back(clip(0.75f, 1.0f));
                                                     return v;
                                                 }())
                                                     .unwrap()};
    MotionAnimation animation = owner(std::move(tree));
    animation.set_attribute(SettableAttribute{encode("height"), 1.0f, false, -1});
    animation.jump_into(encode("manualInto")).unwrap();
    CHECK_EQ(animation.current_time().unwrap(), 0.75f);
    CHECK(animation.settable.entries().empty());
    CHECK(animation.motion_attributes.empty());
}

S3_TEST(jump_into_missing_marker_preserves_time_without_emitting_packets) {
    MotionAnimation animation = owner(clip(0.625f, 0.0f));
    animation.advance(0.125f, 0.0f);
    animation.jump_into(encode("missing-marker")).unwrap();
    CHECK_EQ(animation.current_time().unwrap(), 0.125f);
    CHECK(animation.motion_attributes.empty());
}
