// Port of skate-game/src/physics/camera_output_tests.rs (mounted as physics::camera_output::tests).
#include "harness.hpp"
#include "s3/game/physics/camera_output.hpp"

using s3::game::physics::camera_output::offboard_output;
using s3::player::input_phase::OffBoardOutputFields;
using A4 = std::array<float, 4>;

// Host wiring regression, not an independently executed Skate 3 parity test.
S3_TEST(offboard_camera_consumes_the_distinct_completed_output_fields) {
    auto raw = [](A4 v) { return s3::rs::to_bits(v); };
    OffBoardOutputFields physical{};
    physical.scalar_92 = 2.5f;
    physical.scalar_152 = 0.75f;
    physical.scalar_156 = 1.25f;
    physical.vector_160 = raw({0.f, 1.f, 0.f, 0.f});
    physical.vector_176 = raw({1.f, 2.f, 3.f, 1.f});
    physical.vector_192 = raw({0.f, 0.f, 1.f, 0.f});
    physical.vector_208 = raw({4.f, 5.f, 6.f, 1.f});
    physical.vector_224 = raw({1.f, 0.f, 0.f, 0.f});
    physical.vector_240 = raw({7.f, 8.f, 9.f, 1.f});
    physical.trajectory_valid_331 = 1;
    physical.flag_334 = 1;
    physical.flag_308 = 1;
    physical.flag_304 = 0;
    const auto camera = offboard_output(physical);
    CHECK_EQ(camera.duration_92, 2.5f);
    CHECK_EQ(camera.time_152, 0.75f);
    CHECK_EQ(camera.apex_time_156, 1.25f);
    CHECK(camera.launch_normal_160 == (A4{0.f, 1.f, 0.f, 0.f}));
    CHECK(camera.launch_position_176 == (A4{1.f, 2.f, 3.f, 1.f}));
    CHECK(camera.landing_normal_192 == (A4{0.f, 0.f, 1.f, 0.f}));
    CHECK(camera.landing_position_208 == (A4{4.f, 5.f, 6.f, 1.f}));
    CHECK(camera.heading_224 == (A4{1.f, 0.f, 0.f, 0.f}));
    CHECK(camera.apex_240 == (A4{7.f, 8.f, 9.f, 1.f}));
    CHECK_EQ(camera.use_trajectory_331, 1);
    CHECK_EQ(camera.dropping_in_334, 1);
    CHECK_EQ(camera.object_held_304, 1);
}

S3_TEST(reset_offboard_output_does_not_invent_a_valid_trajectory) {
    const auto camera = offboard_output(OffBoardOutputFields{});
    CHECK_EQ(camera.use_trajectory_331, 0);
    CHECK_EQ(camera.dropping_in_334, 0);
    CHECK_EQ(camera.object_held_304, 0);
}
