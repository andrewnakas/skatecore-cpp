// Port of skate-core/src/riding/tests/push.rs.
#include "harness.hpp"
#include "s3/core/riding/push.hpp"

namespace {
using namespace s3::riding::push;

PushInput input() {
    return PushInput{0x02000000u, 0, 10.0f, 0.0f, 0.0f, 0.5f, 0.125f, Vector3{0.0f, 0.0f, 1.0f}};
}
PushLimits limits() { return PushLimits{8.5f, 2.0f, 1.0f}; }

S3_TEST(inactive_and_suppressed_push_clear_both_output_vectors) {
    PushInput i = input();
    i.flags_2468 = 0;
    i.flags_2472 = 0x200;
    CHECK_EQ(calculate_acceleration(i, limits()), PushAcceleration{});
    i.flags_2468 = 0x02000000u;
    PushAcceleration suppressed{};
    suppressed.suppressed = true;
    CHECK_EQ(calculate_acceleration(i, limits()), suppressed);
}

S3_TEST(tu3_blends_the_two_speed_limits_before_time_conversion) {
    CHECK_EQ(calculate_acceleration(input(), limits()).vector.z, 8.0f);
    PushInput i = input();
    i.current_speed = 4.25f;
    i.absolute_body_speed = 4.25f;
    CHECK_EQ(calculate_acceleration(i, limits()).vector.z, 6.0f);
}

S3_TEST(speed_cap_is_based_on_absolute_speed_and_never_brakes) {
    PushInput i = input();
    i.current_speed = 8.25f;
    i.absolute_body_speed = 8.25f;
    CHECK_EQ(calculate_acceleration(i, limits()).vector.z, 1.0f);
    i.absolute_body_speed = 9.0f;
    CHECK_EQ(calculate_acceleration(i, limits()).vector, Vector3::ZERO);
    PushInput j = input();
    j.target_speed = -1.0f;
    CHECK_EQ(calculate_acceleration(j, limits()).vector, Vector3::ZERO);
}

S3_TEST(native_force_direction_keeps_vertical_component_and_zero_application_point) {
    PushInput i = input();
    i.direction = Vector3{0.0f, 0.6f, 0.8f};
    const auto result = calculate_acceleration(i, limits());
    CHECK_EQ(result.vector, (Vector3{0.0f, 4.8f, 6.4f}));
    CHECK_EQ(result.local_point, Vector3::ZERO);
}
}  // namespace
