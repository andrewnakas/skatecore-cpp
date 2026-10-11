// Port of skate-game/src/physics/offboard/settings/tests.rs
#include <cmath>
#include <cstdio>

#include "game_test_assets.hpp"
#include "harness.hpp"
#include "s3/data/animation_banks.hpp"
#include "s3/game/physics/offboard/settings.hpp"

using namespace s3::game::physics::offboard::settings;
using s3::data::ClipAttribute;
using s3::data::ClipMetadata;

namespace {
ClipMetadata clip() {
    ClipMetadata c;
    c.name = "NB_WALK_FWD_CYC";
    c.source_offset = 48;
    c.fps_bits = s3::rs::to_bits(60.0f);
    c.frames_bits = s3::rs::to_bits(57.0f);
    c.base_speed_bits = s3::rs::to_bits(3.0f);
    c.flags_word = 0;
    ClipAttribute a;
    a.name = "ANIMTRANSZ";
    a.type_id = 0;
    a.begin_bits = s3::rs::to_bits(-1.0f);
    a.end_bits = s3::rs::to_bits(-1.0f);
    a.payload_words = {s3::rs::to_bits(1.5f)};
    a.source_offset = 128;
    c.attributes.push_back(a);
    return c;
}
}  // namespace

S3_TEST(query_uses_full_frame_duration_and_negative_untimed_end) {
    const auto metric = *metrics::metric(clip()).unwrap();
    CHECK_EQ(metric.end_time, -0.95f);
    CHECK_EQ(metric.translation_z, 1.5f);
    CHECK(std::fabs(-metric.translation_z / metric.end_time - 1.5789474f) < 1e-6f);
}

S3_TEST(first_matching_record_wins_without_a_playback_status_filter) {
    auto c = clip();
    c.attributes[0].begin_bits = s3::rs::to_bits(0.8f);
    c.attributes[0].end_bits = s3::rs::to_bits(0.5f);
    ClipAttribute later;
    later.name = "ANIMTRANSZ";
    later.type_id = 0;
    later.begin_bits = 0;
    later.end_bits = s3::rs::to_bits(1.0f);
    later.payload_words = {s3::rs::to_bits(99.0f)};
    later.source_offset = 192;
    later.begin_bits = 0;
    c.attributes.push_back(later);
    const auto metric = *metrics::metric(c).unwrap();
    CHECK_EQ(metric.translation_z, 1.5f);
    CHECK_EQ(metric.end_time, 0.475f);
}

S3_TEST(actual_attribute_absence_is_distinct_from_malformed_data) {
    auto c = clip();
    c.attributes[0].type_id = 2;
    CHECK(metrics::metric(c).is_err());
    c.attributes.clear();
    CHECK(!metrics::metric(c).unwrap().has_value());
}

S3_TEST(graph_bounds_are_separate_and_duplicate_knots_preserve_endpoint_jump) {
    const float values[] = {-9.0f, -8.0f, 7.0f, 6.0f, 0.0f, 0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
    std::string hex;
    for (float v : values) {
        char buf[16];
        std::snprintf(buf, sizeof buf, "%08X", s3::rs::to_bits(v));
        hex += buf;
    }
    const auto parsed = curves::parse<3>(hex).unwrap();
    const auto& g = parsed.first;
    const auto bounds = parsed.second;
    CHECK((bounds == std::array<float, 4>{-9.0f, -8.0f, 7.0f, 6.0f}));
    CHECK_EQ(g.evaluate(-0.01f), 2.0f);
    CHECK_EQ(g.evaluate(0.0f), 3.0f);
    CHECK_EQ(g.evaluate(0.5f), 3.5f);
    CHECK_EQ(g.evaluate(1.0f), 4.0f);
    CHECK(curves::parse<4>(hex).is_err());
}

// #[ignore = "requires extracted private stock assets"]
S3_TEST(actual_stock_settings_and_metrics_load_together) {
    const auto* vfs = s3test::game::assets();
    if (vfs == nullptr) return;
    auto collections = s3::data::Collections::load(*vfs, s3test::game::ASSET_ROOT);
    CHECK(collections.ok);
    auto banks = s3::data::AnimationBanks::load(*vfs, s3test::game::ASSET_ROOT);
    CHECK(banks.ok);
    auto metadata = banks.value.metadata();
    CHECK(metadata.ok);
    const auto settings = Settings::load(collections.value, metadata.value).unwrap();
    for (const auto& m : settings.metrics) CHECK(m.has_value());
    std::array<float, 3> speeds{};
    for (std::size_t i = 0; i < 3; ++i) {
        const auto m = *settings.metrics[i];
        speeds[i] = (-1.0f / m.end_time) * m.translation_z;
    }
    CHECK(speeds[0] > 0.0f && speeds[0] < speeds[1] && speeds[1] < speeds[2]);
    CHECK_EQ(settings.controller.movement_intent.sprint_time_cap, 4.0f);
}
