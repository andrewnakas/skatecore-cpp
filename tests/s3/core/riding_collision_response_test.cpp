// Port of the #[cfg(test)] module in skate-core/src/riding/collision_response.rs.
#include "harness.hpp"
#include "s3/core/riding/collision_response.hpp"

namespace {
using namespace s3::riding::collision_response;
using V4 = std::array<float, 4>;

S3_TEST(late_false_still_publishes_collision_target_and_angle) {
    CollisionResponseSettings settings;
    settings.maximum_velocity_delta = 8.0f;
    settings.force_y_offset = -0.05f;
    settings.force_scalar = 1.0f;
    settings.target_displacement_velocity = 1.0f;
    settings.torque_vs_angle = PointGraph<8>{
        {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f},
        {0.03f, 0.03f, 0.03f, 0.03f, 0.03f, 0.03f, 0.03f, 0.03f},
    };
    CollisionResponsePhysical input{
        0x20000,
        {1.0f, 0.0f, 0.0f, 0.0f},
        {2.0f, 0.0f, 4.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        1.0f / 60.0f,
        8.0f,
    };
    const auto late = s3::rs::unwrap(collision_response(settings, input));
    CHECK(!late.applied);
    CHECK_EQ(late.force, (V4{0.0f, 0.0f, 0.0f, 0.0f}));
    CHECK_EQ(late.target_velocity, (V4{1.0f, 0.0f, 4.0f, 0.0f}));
    CHECK(late.angular_displacement[1] > 0.0f);
    input.velocity = {-20.0f, 0.0f, 4.0f, 0.0f};
    const auto clamped = s3::rs::unwrap(collision_response(settings, input));
    CHECK(clamped.applied);
    CHECK(s3::rs::abs(clamped.force[0] - 3840.0f) < 0.001f);
    CHECK_EQ(clamped.point, (V4{0.0f, -0.05f, 0.0f, 0.0f}));
    input.collision_displacement = input.ground_normal;
    CHECK(!collision_response(settings, input).has_value());
}
}  // namespace
