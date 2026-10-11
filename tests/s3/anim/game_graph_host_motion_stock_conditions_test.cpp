// Port of skate-game/src/graph_host/motion_stock_conditions/tests.rs.
#include "assets.hpp"
#include "harness.hpp"
#include "s3/data/animation_banks.hpp"
#include "s3/data/result.hpp"
#include "s3/game/graph_host/motion_animation.hpp"
#include "s3/game/graph_host/motion_stock_conditions.hpp"
#include "s3/game/graph_runtime/disk.hpp"

using namespace s3::game::graph_host::motion_stock_conditions;
using s3::game::graph_host::motion_animation::MotionAnimation;

S3_TEST(obstacle_distance_uses_named_clip_displacement_and_native_boundary) {
    CHECK(!enough_distance(0.49f, 0.2f));
    CHECK(enough_distance(0.5f, 0.2f));
    CHECK(enough_distance(0.51f, 0.2f));
    CHECK(!enough_distance(0.29f, 0.0f));
    CHECK(enough_distance(0.3f, 0.0f));
    //Native fcmpu/blt; this leaf does not substitute a finite-value guard.
    CHECK(enough_distance(s3::rs::f32::NAN_, 0.2f));
}

S3_TEST(stop_animation_query_uses_database_and_ignores_event_time_gate) {
    auto metadata = s3::data::AnimationMetadata::parse(R"({
        "version":1,"source_bank":"OffBoard.abin",
        "source_sha256":"0000000000000000000000000000000000000000000000000000000000000000",
        "source_bytes":256,"clips":[{
            "name":"TEST_STOP","source_offset":48,"fps_bits":1114636288,
            "frames_bits":1110704128,"base_speed_bits":1065353216,"flags_word":0,
            "attributes":[{"name":"ANIMTRANSZ","type_id":0,
                "begin_bits":1056964608,"end_bits":1065353216,
                "payload_words":[1056964608],"source_offset":128}]
        }],"unsupported_trees":[]
    })");
    if (!metadata.ok) s3test::fail(__FILE__, __LINE__, metadata.error);
    const MotionAnimation animation = MotionAnimation::from_metadata(std::move(metadata.value));
    CHECK_EQ(animation.stock_clip_translation_z("offboard", "test_stop").unwrap(), 0.5f);
    CHECK_EQ(animation.stock_clip_translation_z("OnBoard", "TEST_STOP").unwrap(), 0.0f);
    CHECK_EQ(animation.stock_clip_translation_z("OffBoard", "MISSING_STOP").unwrap(), 0.0f);
}

// #[ignore = "requires private stock banks; named stop-clip predicate regression, not gameplay acceptance"]
S3_TEST(stock_stop_clips_drive_obstacle_distance_predicate) {
    S3_REQUIRE_ASSETS(root);
    const s3::data::Vfs vfs = s3::game::graph_runtime::disk::read_banks(root);
    auto banks = s3::data::AnimationBanks::load(vfs, root);
    if (!banks.ok) s3test::fail(__FILE__, __LINE__, banks.error);
    auto metadata = banks.value.metadata();
    if (!metadata.ok) s3test::fail(__FILE__, __LINE__, metadata.error);
    auto animation_metadata = banks.value.metadata();
    if (!animation_metadata.ok) s3test::fail(__FILE__, __LINE__, animation_metadata.error);
    const MotionAnimation animation = MotionAnimation::from_metadata(std::move(animation_metadata.value));
    //Every authored EnoughDistToObstacle dependency in OffBoard/intostand.xml.
    for (const char* prefix : {"WALK", "RUN"}) {
        for (const int phase : {0, 25, 50, 75}) {
            const std::string name =
                std::string("BR_") + prefix + "_FWD_" + std::to_string(phase) + "_INTO_STAND_0";
            auto clip = metadata.value.clip(name);
            if (!clip.ok) s3test::fail(__FILE__, __LINE__, clip.error);
            const s3::data::ClipAttribute* attribute = nullptr;
            for (const auto& a : clip.value->attributes) {
                if (s3::data::eq_ignore_ascii_case(a.name, "AnimTransZ")) {
                    attribute = &a;
                    break;
                }
            }
            CHECK_MSG(attribute != nullptr, "Authored stopping displacement");
            CHECK_MSG(attribute->type_id == 0, name);
            const float displacement = s3::rs::from_bits(attribute->payload_words[0]);
            CHECK_EQ(animation.stock_clip_translation_z("OffBoard", name).unwrap(), displacement);
            const float boundary = displacement + s3::rs::from_bits(0x3e99999au);
            CHECK_MSG(enough_distance(boundary, displacement), name);
            CHECK_MSG(!enough_distance(boundary - 0.001f, displacement), name);
        }
    }
}
