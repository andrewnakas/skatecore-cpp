// Port of the inline tests in skate-core/src/riding/ground_correction_math.rs.
#include "harness.hpp"
#include "s3/core/riding/ground_correction_math.hpp"

namespace {
using namespace s3::riding::ground_correction_math;
using V4 = std::array<float, 4>;

S3_TEST(corrections_preserve_native_axes_and_units) {
    const auto force = hang_force({0.f, 0.f, 0.f, 0.f}, {0.f, 0.f, 2.f, 0.f}, {1.f, 0.f, 0.f, 0.f});
    CHECK(s3::rs::abs(force[0] - 50.f) < 0.0001f);
    CHECK_EQ(force[1], 160.f);
    CHECK_EQ(edge_up({0.f, 1.f, 0.f, 0.f}), (V4{1.f, 0.f, 0.f, 0.f}));
    CHECK_EQ(collision_force_projection({0.f, 0.f, 4.f, 0.f}, {0.f, 0.f, -3.f, 0.f}), -3.f);
    CHECK_EQ(collision_force_projection({0.f, 0.f, 0.f, 0.f}, {1.f, 2.f, 3.f, 0.f}), 0.f);
    const auto pin = pinning_velocity({4.f, 9.f, 8.f, 0.f}, 3.f, 10.f, 1.f / 60.f);
    CHECK(s3::rs::abs(pin[0] + 60.f) < 0.00001f);
    CHECK_EQ(pin[1], 0.f);
    CHECK(s3::rs::abs(pin[2] - 120.f) < 0.00002f);
    CHECK(wheel_catch_displacement({0.f, 0.f, 1.f, 0.f}, {0.f, 1.f, 0.f, 0.f})[0] > 0.f);
}

}  // namespace
