// Port of skate-core/src/riding/tests/speed_and_slope.rs.
#include "harness.hpp"
#include "s3/core/riding/speed_and_slope.hpp"

namespace {
using namespace s3::riding::speed_and_slope;
using s3::point_graph::PointGraph;

SpeedAndSlopeSettings linear_settings() {
    const std::array<float, 8> keys{0.0f, 0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 1.0f};
    SpeedAndSlopeSettings s;
    s.turn_torque_vs_speed = PointGraph<8>{keys, keys};
    s.turn_torque_vs_slope = PointGraph<8>{keys, keys};
    s.heading_adjust_max_speed = 8.0f;
    return s;
}

S3_TEST(coefficient_uses_absolute_forward_speed_and_saturates_its_fraction) {
    const auto settings = linear_settings();
    const float half_speed = settings.calculate(0.0f, 4.0f);
    CHECK(s3::rs::abs(half_speed - 0.5f) < 0.00001f);
    CHECK_EQ(settings.calculate(0.0f, -4.0f), half_speed);
    CHECK(s3::rs::abs(settings.calculate(0.0f, 80.0f) - 1.0f) < 0.00001f);
    CHECK_EQ(settings.calculate(0.0f, 0.0f), 0.0f);
}

S3_TEST(over_vertical_slope_returns_to_the_first_curve_key) {
    auto settings = linear_settings();
    settings.turn_torque_vs_slope.y[0] = 0.375f;
    // acos(-1)/(pi/2) exceeds1.2. The native cutoff selects zero,
    // so an inverted board uses the first graph value, not its last value.
    CHECK_EQ(settings.calculate(-1.0f, 8.0f), 0.375f);
    CHECK_EQ(settings.calculate(-2.0f, 8.0f), 0.375f);
    CHECK(s3::rs::abs(settings.calculate(0.0f, 8.0f) - 1.0f) < 0.00001f);
}

}  // namespace
