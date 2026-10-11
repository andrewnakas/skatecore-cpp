// Port of skate-core/tests/animation_packet_merge.rs (crate integration test).
#include "harness.hpp"
#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/output/motion_graph_packet.hpp"

using namespace s3::animation::output::attributes;
using namespace s3::animation::output::motion_graph_packet;
namespace motion_graph_packet = s3::animation::output::motion_graph_packet;
using OptWord = std::optional<std::uint32_t>;
using Payload = std::array<OptWord, 6>;

namespace {
AnimationAttribute attribute(std::uint8_t kind, std::uint32_t seed) {
    AnimationAttribute a;
    for (std::size_t i = 0; i < 6; ++i) a.payload._0[i] = seed + static_cast<std::uint32_t>(i);
    a.begin_time = 2.0f;
    a.end_time = 3.0f;
    a.name = AttributeName{{seed, seed, seed, seed, seed}};
    a.status = 9;
    a.kind = kind;
    a.sequence_id = 17;
    return a;
}
MotionGraphPacket motion(std::uint32_t seed) {
    MotionGraphPacket p;
    p.trick.identifiers = {TrickIdentifier{{seed, seed, seed, seed, seed, seed}},
                           TrickIdentifier{{seed + 1, seed + 1, seed + 1, seed + 1, seed + 1, seed + 1}}};
    p.trick.word48 = seed + 2;
    p.trick.scalars52_56 = {s3::rs::from_bits(0x80000000u), s3::rs::from_bits(0x7fc12345u)};
    p.gesture.gesture = seed + 3;
    p.gesture.flag_bytes = {1, 2, 0xaa, 0xbb};
    p.wipeout_gesture = {4.0f, -5.0f};
    return p;
}
}  // namespace

S3_TEST(assignment_copies_only_kind_selected_union_lanes_and_all_metadata) {
    for (std::uint8_t kind : {0, 1, 2, 3, 4, 255}) {
        const AnimationAttribute source = attribute(kind, 100);
        AnimationAttribute destination = attribute(3, 200);
        destination.copy_from(source);
        std::size_t count = 0;
        switch (kind) {
            case 0:
            case 2: count = 1; break;
            case 1: count = 4; break;
            case 3: count = 6; break;
            default: count = 0; break;
        }
        for (std::size_t lane = 0; lane < 6; ++lane) {
            CHECK(destination.payload._0[lane] ==
                  OptWord((lane < count ? 100u : 200u) + static_cast<std::uint32_t>(lane)));
        }
        CHECK_EQ(destination.kind, kind);
        CHECK(destination.name == source.name);
        CHECK_EQ(destination.begin_time, 2.0f);
        CHECK_EQ(destination.end_time, 3.0f);
        CHECK_EQ(destination.status, std::uint8_t(9));
        CHECK_EQ(destination.sequence_id, 17);
    }
}

S3_TEST(motion_graph_conversion_preserves_scalar_bits_without_fabricating_other_lanes) {
    for (std::uint32_t bits : {0x80000000u, 0x7fc12345u, 0x3f800000u}) {
        const MotionGraphAttribute source{AttributeName{{10, 20, 30, 40, 50}}, s3::rs::from_bits(bits)};
        const AnimationAttribute result = source.to_animation();
        CHECK((result.payload._0 == Payload{bits, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt}));
        CHECK(result.name == source.name);
        CHECK_EQ(result.begin_time, -1.0f);
        CHECK_EQ(result.end_time, -1.0f);
        CHECK_EQ(result.status, std::uint8_t(6));
        CHECK_EQ(result.kind, std::uint8_t(0));
        CHECK_EQ(result.sequence_id, -1);
    }
}

S3_TEST(merge_resets_active_length_preserves_duplicates_and_keeps_mg_before_tree) {
    const AttributeName name{{1, 2, 3, 4, 5}};
    const std::array<MotionGraphAttribute, 2> mg = {MotionGraphAttribute{name, 0.0f}, MotionGraphAttribute{name, 2.0f}};
    std::array<AnimationAttribute, 2> tree = {attribute(0, 7), attribute(1, 8)};
    tree[0].name = name;
    PacketAttributes list{};
    list.append(attribute(3, 500));
    list.replace_from(mg, tree);
    CHECK_EQ(list.entries().len(), std::size_t(4));
    CHECK(list.entries()[0].name == name);
    CHECK(list.entries()[1].name == name);
    CHECK(list.entries()[2].name == name);
    CHECK(list.entries()[0].payload._0[0] == OptWord(0u));
    CHECK(list.entries()[1].payload._0[0] == OptWord(s3::rs::to_bits(2.0f)));
    CHECK(list.entries()[2].payload._0[0] == OptWord(7u));
    CHECK_EQ(list.entries()[3].kind, std::uint8_t(1));
    list.replace_from({}, {});
    CHECK(list.entries().is_empty());
    list.append(attribute(0, 900));
    CHECK((list.entries()[0].payload._0 == Payload{900u, 501u, 502u, 503u, 504u, 505u}));
}

S3_TEST(concrete_motion_graph_publication_preserves_source_including_gesture_and_trick) {
    const MotionGraphPacket source = motion(10);
    MotionGraphPacket destination = motion(90);
    motion_graph_packet::publish(source, destination);
    CHECK(destination.trick.identifiers == source.trick.identifiers);
    CHECK_EQ(destination.trick.word48, 12u);
    CHECK_EQ(s3::rs::to_bits(destination.trick.scalars52_56[0]), 0x80000000u);
    CHECK_EQ(s3::rs::to_bits(destination.trick.scalars52_56[1]), 0x7fc12345u);
    CHECK(destination.gesture == source.gesture);
    CHECK((destination.wipeout_gesture == std::array<float, 2>{4.0f, -5.0f}));
    CHECK_EQ(source.gesture.gesture, 13u);
    CHECK((source.wipeout_gesture == std::array<float, 2>{4.0f, -5.0f}));
    destination.gesture.gesture = 37;
    motion_graph_packet::publish(source, destination);
    CHECK_EQ(destination.gesture.gesture, 13u);
}
