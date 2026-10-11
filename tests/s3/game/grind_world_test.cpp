// Port of skate-game/src/grind_world/tests.rs (mounted as grind_world::tests).
#include <cstdio>

#include "harness.hpp"
#include "s3/game/grind_world.hpp"

using namespace s3::game::grind_world;
using DRail = s3::data::Rail;

namespace {

void le64(std::vector<std::uint8_t>& out, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) out.push_back(std::uint8_t(v >> (8 * i)));
}
void le32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(std::uint8_t(v >> (8 * i)));
}

DRail native(std::uint64_t id, float length) {
    std::vector<std::uint8_t> raw;
    le64(raw, id);
    le64(raw, 0x2c70170700060230ULL);
    for (std::uint32_t word : {0u, 0u, 1u}) le32(raw, word);
    std::array<std::uint32_t, 30> segment{};
    segment[0] = s3::rs::to_bits(-2.0f * length);
    segment[4] = s3::rs::to_bits(3.0f * length);
    segment[15] = s3::rs::to_bits(1.0f);
    segment[24] = s3::rs::to_bits(length);
    for (std::uint32_t word : segment) le32(raw, word);
    DRail r;
    r.name = "owned test rail";
    r.closed = false;
    r.points = {};
    r.native = raw;
    return r;
}

}  // namespace

S3_TEST(no_map_keeps_current_flat_course_empty) {
    CHECK(primitives(nullptr).unwrap().empty());
    auto provider = StaticProvider::new_(nullptr).unwrap();
    CHECK(provider.query({-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}).unwrap().empty());
}

S3_TEST(native_tiny_and_duplicate_knots_are_not_dropped) {
    const auto bytes = spline::build_rails({native(1, 0.0f), native(2, 0.0001f)}).unwrap();
    const auto decoded = spline::decoded_from_blob(bytes).unwrap();
    const auto& entries = decoded.first;
    const auto& metadata = decoded.second;
    CHECK_EQ(entries.size(), std::size_t(2));
    CHECK(entries[0].start == entries[0].end);
    CHECK_EQ(metadata[0].flags & 0x80000000u, 0x80000000u);
    CHECK(entries[1].end[0] > 0.0f);
    CHECK_EQ(metadata[1].segment_index, 0u);
}

S3_TEST(same_native_spline_id_does_not_alias_header_instances) {
    const auto bytes = spline::build_rails({native(0x1234, 1.0f), native(0x1234, 2.0f)}).unwrap();
    const auto decoded = spline::decoded_from_blob(bytes).unwrap();
    const auto& entries = decoded.first;
    const auto& metadata = decoded.second;
    CHECK(metadata[0].spline_guids == metadata[1].spline_guids);
    CHECK((metadata[0].spline_guids == std::array<std::uint64_t, 2>{0x1234, 0x2c70170700060230ULL}));
    CHECK_NE(entries[0].owner, entries[1].owner);
}

S3_TEST(provider_keeps_both_header_guids_and_source_rail_identity) {
    using s3::data::Extension;
    using s3::data::SkateMap;
    // Synthetic numeric records exercise the stock package layout without
    // embedding private stock bytes. The second GUID is not an asset ID.
    std::vector<DRail> rails = {native(0x1234, 1.0f), native(0x1234, 2.0f)};
    const std::uint64_t second_guid = 0x1020304050607080ULL;
    for (int i = 0; i < 8; ++i) (*rails[1].native)[std::size_t(8 + i)] = std::uint8_t(second_guid >> (8 * i));
    std::string records = "[";
    for (std::size_t index = 0; index < rails.size(); ++index) {
        rails[index].name = "0xAA_9_" + std::to_string(index + 7);
        char sig[32];
        if (index == 0) {
            std::snprintf(sig, sizeof sig, "0x2c70170700060230");
        } else {
            std::snprintf(sig, sizeof sig, "0x%016llx", static_cast<unsigned long long>(second_guid));
        }
        if (index > 0) records += ",";
        records += std::string("{\"asset_id\":\"0xAA\",\"closed\":false,\"flags\":0,\"rail_index\":") +
                   std::to_string(index + 7) +
                   ",\"section_index\":9,\"section_offset\":128,\"segment_count\":1,\"spline_id\":\"0x1234\","
                   "\"stream_file\":\"synthetic.xsf\",\"trailing_word\":0,\"type_signature\":\"" +
                   sig + "\"}";
    }
    records += "]";
    const std::string payload =
        "{\"grind_coordinate_policy\":{\"mode\":\"world_space\"},\"grind_splines\":" + records + "}";
    SkateMap map;
    map.version = 14;
    map.name = "metadata fixture";
    map.rails = rails;
    Extension ext;
    ext.tag = {'W', 'M', 'E', 'T'};
    ext.schema = 1;
    ext.payload.assign(payload.begin(), payload.end());
    map.extensions.push_back(ext);
    auto provider = StaticProvider::new_(&map).unwrap();
    const auto& entries = provider.primitives();
    CHECK((provider.metadata(1)->spline_guids == std::array<std::uint64_t, 2>{0x1234, second_guid}));
    CHECK((provider.spline_guids(entries[1].owner) == std::array<std::uint64_t, 2>{0x1234, second_guid}));
    CHECK(provider.spline_guids(entries[0].owner) != provider.spline_guids(entries[1].owner));
    CHECK(provider.source_rail_index(0) == std::optional<std::uint64_t>(7));
    CHECK(provider.source_rail_index(1) == std::optional<std::uint64_t>(8));
    CHECK_EQ(provider.source(1)->asset_id, std::string("0xAA"));
    CHECK_EQ(provider.metadata(1)->flags, 0u);
    CHECK(!provider.spline_guids(0).has_value());
    CHECK(!provider.spline_guids(UINT64_MAX).has_value());
    CHECK(provider.metadata(2) == nullptr);
}

S3_TEST(vertical_flag_uses_horizontal_tolerance_not_chord_length) {
    const float epsilon = s3::rs::from_bits(0x37800000u);
    const auto bytes = spline::build_rails({native(1, epsilon), native(2, epsilon * 2.0f)}).unwrap();
    const auto metadata = spline::decoded_from_blob(bytes).unwrap().second;
    CHECK_EQ(metadata[0].flags, 0x80000000u);
    CHECK_EQ(metadata[1].flags, 0u);
}

S3_TEST(native_payload_and_inclusive_link_bounds_survive_conversion) {
    const DRail rail = native(0x123456789abcdef0ULL, 1.0f);
    const auto bytes = spline::build_rails({rail}).unwrap();
    auto word = [&](std::size_t o) {
        return (std::uint32_t(bytes[o]) << 24) | (std::uint32_t(bytes[o + 1]) << 16) |
               (std::uint32_t(bytes[o + 2]) << 8) | std::uint32_t(bytes[o + 3]);
    };
    CHECK_EQ(word(16), 0x12345678u);
    CHECK_EQ(word(20), 0x9abcdef0u);
    CHECK_EQ(word(36), 48u);
    CHECK_EQ(word(40), 48u);
    CHECK_EQ(word(48 + 120), 16u);
    CHECK_EQ(word(48 + 124), 0u);
    CHECK_EQ(word(48 + 128), 0u);
}

S3_TEST(octree_resident_head_insertion_and_forty_cap) {
    using octree::Bounds;
    using octree::Octree;
    const Bounds b{{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}};
    const auto tree = Octree::new_(b, std::vector<Bounds>(50, b)).unwrap();
    std::vector<std::size_t> expected;
    for (std::size_t i = 50; i-- > 10;) expected.push_back(i);
    CHECK(tree.query(b, 40) == expected);
    CHECK(tree.query(b, 0).empty());
}

S3_TEST(identity_bounds_transform_retains_native_center_rounding) {
    using octree::Bounds;
    const Bounds authored{{16777216.0f, 16777216.0f, 16777216.0f}, {16777218.0f, 16777218.0f, 16777218.0f}};
    const Bounds query_bounds = authored.identity_transformed();
    CHECK((query_bounds.min == std::array<float, 3>{16777215.0f, 16777215.0f, 16777215.0f}));
    CHECK((query_bounds.max == std::array<float, 3>{16777216.0f, 16777216.0f, 16777216.0f}));
}

S3_TEST(native_split_redistributes_old_linked_list_before_next_insertion) {
    using octree::Bounds;
    using octree::Octree;
    const Bounds b{{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}};
    const Bounds point{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    // Fourth movable entry causes one split; fifth causes a second. Each
    // redistribution walks the old head and prepends, reversing that list.
    const auto tree = Octree::new_(b, std::vector<Bounds>(5, point)).unwrap();
    CHECK((tree.query(b, 40) == std::vector<std::size_t>{3, 2, 1, 0, 4}));
}

S3_TEST(malformed_native_counts_and_inverted_authored_bounds_are_errors) {
    DRail rail = native(1, 1.0f);
    for (int i = 0; i < 4; ++i) (*rail.native)[std::size_t(24 + i)] = std::uint8_t(2u >> (8 * i));
    CHECK(spline::build_rails({rail}).is_err());
    rail = native(1, 1.0f);
    const std::uint32_t two = s3::rs::to_bits(2.0f);
    for (int i = 0; i < 4; ++i) (*rail.native)[std::size_t(28 + 80 + i)] = std::uint8_t(two >> (8 * i));
    CHECK(spline::build_rails({rail}).is_err());
}
