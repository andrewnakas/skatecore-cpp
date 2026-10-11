// Port of skate-core/src/physics/tests/joint_records.rs.
#include <utility>

#include "harness.hpp"
#include "s3/core/physics/joint_records.hpp"

namespace {
using namespace s3::physics::joint_records;
using Pair = std::pair<BodyId, BodyId>;

S3_TEST(raw_record_sizes_match_tu3_owner_strides) {
    CHECK_EQ(sizeof(RetailJointParametersRaw), std::size_t(64));
    CHECK_EQ(sizeof(RetailJointFramesRaw), std::size_t(80));
}

S3_TEST(stock_records_preserve_unrolled_definition_and_live_pair_order) {
    const auto joints = default_joint_records();
    const auto definitions =
        s3::rs::map(joints, [](const RetailJointRecord& j) { return Pair{j.definition_body_0, j.definition_body_1}; });
    CHECK(definitions == (std::array<Pair, 6>{
                             Pair{BodyId::Deck, BodyId::FrontTruck},
                             Pair{BodyId::Deck, BodyId::BackTruck},
                             Pair{BodyId::FrontTruck, BodyId::RightFrontWheel},
                             Pair{BodyId::FrontTruck, BodyId::LeftFrontWheel},
                             Pair{BodyId::BackTruck, BodyId::RightBackWheel},
                             Pair{BodyId::BackTruck, BodyId::LeftBackWheel},
                         }));
    const auto live =
        s3::rs::map(joints, [](const RetailJointRecord& j) { return Pair{j.live_body_a(), j.live_body_b()}; });
    CHECK(live == (std::array<Pair, 6>{
                      Pair{BodyId::FrontTruck, BodyId::Deck},
                      Pair{BodyId::BackTruck, BodyId::Deck},
                      Pair{BodyId::RightFrontWheel, BodyId::FrontTruck},
                      Pair{BodyId::LeftFrontWheel, BodyId::FrontTruck},
                      Pair{BodyId::RightBackWheel, BodyId::BackTruck},
                      Pair{BodyId::LeftBackWheel, BodyId::BackTruck},
                  }));
}

S3_TEST(stock_parameter_arithmetic_matches_create_joints_words) {
    const auto joints = default_joint_records();
    CHECK_EQ(joints[0].parameters.words,
             (std::array<std::uint32_t, 16>{0, 0, 0, 0, 0, 0, 0, 0, 0x42B453D1u, 0, 0, 0x3E7A35DDu, 0x3F800000u,
                                            0x3F78654Du, 0, 1}));
    CHECK_EQ(joints[2].parameters.words,
             (std::array<std::uint32_t, 16>{0, 0, 0, 0, 0, 0, 0, 0, 0, 0x497FA9D8u, 0, 0, 0x3F800000u, 0x3F800000u,
                                            3, 0}));
}

S3_TEST(stock_frame_construction_matches_native_payloads) {
    const auto joints = default_joint_records();
    CHECK_EQ(joints[0].frames.words,
             (std::array<std::uint32_t, 20>{0, 0, 0, 0x3F800000u, 0, 0, 0, 0, 0xBE418051u, 0xBF2E6F8Du, 0x3E41804Fu,
                                            0x3F2E6F8Du, 0, 0xBD676C8Bu, 0x3E78D4FDu, 0, 0xBE418051u, 0xBF2E6F8Du,
                                            0x3E41804Fu, 0x3F2E6F8Du}));
    CHECK_EQ(joints[1].frames.words,
             (std::array<std::uint32_t, 20>{0, 0, 0, 0x3F800000u, 0, 0, 0, 0, 0x3E418051u, 0x3F2E6F8Du, 0x3E41804Fu,
                                            0x3F2E6F8Du, 0, 0xBD676C8Bu, 0xBE78D4FDu, 0, 0x3E418051u, 0x3F2E6F8Du,
                                            0x3E41804Fu, 0x3F2E6F8Du}));
    const std::array<std::uint32_t, 4> wheel_orientation{0x3F3504F3u, 0, 0, 0x3F3504F3u};
    auto slice4 = [&](std::size_t start) {
        return std::array<std::uint32_t, 4>{joints[2].frames.words[start], joints[2].frames.words[start + 1],
                                            joints[2].frames.words[start + 2], joints[2].frames.words[start + 3]};
    };
    CHECK_EQ(slice4(0), wheel_orientation);
    CHECK_EQ(slice4(8), wheel_orientation);
    CHECK_EQ(slice4(16), wheel_orientation);
    CHECK_EQ(joints[2].frames.words[14], 0x3DC28F5Cu);
    CHECK_EQ(joints[3].frames.words[14], 0xBDC28F5Cu);
    CHECK_EQ(joints[4].frames, joints[2].frames);
    CHECK_EQ(joints[5].frames, joints[3].frames);
}
}  // namespace
