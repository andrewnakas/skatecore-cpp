// Port of the inline tests in skate-core/src/physics/board_input_output.rs.
#include "harness.hpp"
#include "s3/core/physics/board_input_output.hpp"

namespace {
using namespace s3::physics::board_input_output;

S3_TEST(forward_and_reverse_motion_use_the_same_travel_axis) {
    const Vector3 forward{0.0f, 0.0f, 1.0f};
    const Vector3 normal{0.0f, 1.0f, 0.0f};
    const float a = travel_angle(forward, Vector3{1.0f, 0.0f, 4.0f}, normal, normal);
    const float b = travel_angle(forward, Vector3{-1.0f, 0.0f, -4.0f}, normal, normal);
    CHECK(s3::rs::abs(a - b) < 0.0001f);
    CHECK(a > 0.0f && a < 0.5f);
    CHECK_EQ(travel_angle(forward, Vector3::ZERO, normal, normal), 0.0f);
}
}  // namespace
