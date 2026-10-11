// Port of skate-game/src/graph_host/motion_grind/animation_owner_tests.rs
// (tests of `motion_animation::grind_owner`, #[path = "motion_grind/animation_owner.rs"]).
#include "harness.hpp"
#include "s3/core/animation/phase_blend.hpp"
#include "s3/core/animation/playback_clip.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/game/graph_host/motion_animation/grind_owner.hpp"
#include "s3/game/graph_host/motion_grind/animation.hpp"

using namespace s3::game::graph_host::motion_animation;
using s3::animation::phase_blend::PhaseBlend;
using s3::animation::playback_clip::ClipAttribute;
using s3::animation::playback_clip::PlaybackClip;
using s3::animation::skeleton_input::name::encode;
using s3::game::graph_host::motion_grind::animation::endpoints;

namespace {
MotionAnimation owner() {
    auto metadata = AnimationMetadata::parse(R"({
        "version":1,"source_bank":"synthetic-test",
        "source_sha256":"0000000000000000000000000000000000000000000000000000000000000000",
        "source_bytes":48,"clips":[],"unsupported_trees":[]
    })");
    if (!metadata.ok) s3test::fail(__FILE__, __LINE__, metadata.error);
    MotionAnimation owner = MotionAnimation::from_metadata(std::move(metadata.value));
    auto clip = [](float value) {
        return PlaybackTree(PlaybackTree::Clip{
            "independent-grind-test",
            PlaybackClip::new_(61.0f, 60.0f, 1.0f, 0,
                               {ClipAttribute{encode("twist"), 0, -1.0f, -1.0f, {s3::rs::to_bits(value)}}})});
    };
    std::vector<PlaybackTree> children;
    children.push_back(clip(-2.0f));
    children.push_back(clip(4.0f));
    owner.current = PlaybackTree(PlaybackTree::PhaseBlend{PhaseBlend::new_(encode("twist"), std::move(children)).unwrap()});
    return owner;
}
}  // namespace

S3_TEST(actual_motion_animation_adapter_uses_live_queue_root_and_packet_owner) {
    MotionAnimation owner_ = owner();
    owner_.motion_intents.insert("unchanged", 0.25f);
    // The probe must replace this queued attribute, not append a duplicate
    // that an actual PhaseBlend would read first and use for both endpoints.
    owner_.set_attribute(SettableAttribute{encode("twist"), 0.5f, true, -1});
    CHECK_EQ(endpoints(owner_, encode("twist")).unwrap(), (std::array<float, 2>{-2.0f, 4.0f}));
    CHECK(!owner_.pending_parameter(encode("twist")).has_value());
    AnimationAttribute out = MotionGraphAttribute{encode("twist"), 0.0f}.to_animation();
    CHECK(owner_.query_current_attribute(encode("twist"), 15, out).unwrap());
    CHECK_EQ(s3::rs::from_bits(*out.payload._0[0]), 4.0f);
    owner_.emit_packet(encode("GrindFacingForwards"), 1.0f);
    CHECK_EQ(owner_.motion_attributes.size(), std::size_t(1));
    CHECK_EQ(owner_.motion_intents.len(), std::size_t(1));
    const float* unchanged = owner_.motion_intents.get("unchanged");
    CHECK(unchanged != nullptr && *unchanged == 0.25f);
}

S3_TEST(actual_owner_missing_tree_is_an_error_not_fabricated_endpoints) {
    MotionAnimation owner_ = owner();
    owner_.current = std::nullopt;
    CHECK(endpoints(owner_, encode("twist")).is_err());
}
