// Port of skate-game/src/physics/riding_outputs/ground_input_tests.rs.
#include "harness.hpp"
#include "s3/game/physics/riding_outputs/ground_input.hpp"

using s3::game::physics::riding_outputs::ground_input::GroundPacketInputs;
using s3::math::Vector3;
using s3::player::input_phase::ProcessedPhysicsInput;

S3_TEST(ground_reckoning_keeps_retained_dynamic_up_separate_from_wheel_support) {
    ProcessedPhysicsInput packet{};
    packet.vectors_464_480_496_512_528[0] = s3::rs::to_bits(std::array<float, 4>{0.0f, 1.0f, 0.0f, 0.0f});
    packet.vectors_464_480_496_512_528[4] = s3::rs::to_bits(std::array<float, 4>{0.6f, 0.8f, 0.0f, 0.0f});
    packet.scalar_2652 = 7.0f;
    packet.scalar_2612 = -3.0f;
    packet.scalar_2616 = 5.0f;
    packet.wheel_count_2556 = 2;
    const auto input = GroundPacketInputs::from_processed(packet);
    CHECK(input.wheel_normal == Vector3::new_(0.0f, 1.0f, 0.0f));
    CHECK(input.dynamic_up == Vector3::new_(0.6f, 0.8f, 0.0f));
    CHECK_EQ(input.speed, 7.0f);
    CHECK_EQ(input.absolute_speed, 5.0f);
    CHECK_EQ(input.wheel_count, 2);

    // A lost-contact packet must retain its published528, not choose UP or
    // the current wheel normal merely because this frame has no support.
    packet.wheel_count_2556 = 0;
    const auto lost = GroundPacketInputs::from_processed(packet);
    CHECK(lost.dynamic_up == input.dynamic_up);
    CHECK_EQ(lost.wheel_count, 0);
}
