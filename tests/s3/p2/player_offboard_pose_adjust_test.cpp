// Port of skate-core/src/player/offboard/pose_adjust/tests.rs.
#include "harness.hpp"
#include "s3/core/player/offboard/pose_adjust.hpp"

namespace {
using namespace s3::player::offboard::pose_adjust;
using s3::physics::skeleton_animation_record::IDENTITY;
using V = std::array<float, 4>;

S3_TEST(mapped_hand_choice_and_relative_translation) {
    CHECK_EQ(selected_hand(4), std::size_t(0));
    CHECK_EQ(selected_hand(0), std::size_t(1));
    auto actual = IDENTITY;
    actual[3] = {7.0f, 8.0f, 9.0f, 0.0f};
    auto reparented = IDENTITY;
    reparented[3] = {2.0f, 3.0f, 4.0f, 0.0f};
    const auto result = adjustment(actual, reparented);
    CHECK_EQ(result[3], (V{5.0f, 5.0f, 5.0f, 0.0f}));
    for (int k = 0; k < 3; ++k) CHECK_EQ(result[k], IDENTITY[k]);
}

S3_TEST(relative_rotated_hand_maps_reparented_position_to_actual_position) {
    auto target = IDENTITY;
    target[0] = {0.0f, 1.0f, 0.0f, 0.0f};
    target[1] = {-1.0f, 0.0f, 0.0f, 0.0f};
    target[3] = {2.0f, 3.0f, 0.0f, 0.0f};
    const auto result = adjustment(IDENTITY, target);
    CHECK_EQ(result[0], (V{0.0f, -1.0f, 0.0f, 0.0f}));
    CHECK_EQ(result[1], (V{1.0f, 0.0f, 0.0f, 0.0f}));
    CHECK_EQ(result[3], (V{-3.0f, 2.0f, 0.0f, 0.0f}));
}
}  // namespace
