// Port of the inline tests in skate-core/src/player/slide_state/forces.rs.
#include "harness.hpp"
#include "s3/core/player/slide_state.hpp"

namespace {
using namespace s3::player::slide_state;
using s3::point_graph::PointGraph;

PointGraph<8> curve(float value) {
    PointGraph<8> g;
    g.x = {0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f};
    g.y.fill(value);
    return g;
}
SlideSettings settings() {
    SlideSettings s;
    s.input_remap = curve(0.0f);
    s.remap_vs_speed = curve(0.0f);
    s.force_vs_angle = curve(0.0f);
    s.force_vs_speed = curve(0.0f);
    s.softest_wheel_force = 0.25f;
    s.softest_wheel_spin = 1.0f;
    s.angular_force = 0.1f;
    s.force_y_offset = -0.12f;
    return s;
}
SlideInput input() {
    SlideInput p;
    p.velocity = {4.f, 0.f, 3.f, 0.f};
    p.normal = {0.f, 1.f, 0.f, 0.f};
    p.side = {1.f, 0.f, 0.f, 0.f};
    p.effective_forward = {0.f, 0.f, 1.f, 0.f};
    p.reference_forward = {0.f, 0.f, 1.f, 0.f};
    p.angular_velocity = {};
    p.absolute_speed = 5.f;
    p.surface_speed = 5.f;
    p.slide = 0.f;
    p.elapsed = 0.f;
    p.wheel_hardness = 0.5f;
    return p;
}

S3_TEST(negative_slide_force_bypasses_wheel_softness_and_reverses_application_height) {
    const SlideSettings s = settings();
    const SlideInput p = input();
    SlideSurface surface{curve(2.f), 0.f, 0.f};
    const auto positive = sliding_force(s, surface, p);
    CHECK(s3::rs::abs(positive.force_world.x - 3.2f) < 0.00001f);
    CHECK(s3::rs::abs(positive.force_world.z - 2.4f) < 0.00001f);
    CHECK_EQ(positive.point_body.y, -0.12f);
    surface.speed_to_force = curve(-2.f);
    const auto negative = sliding_force(s, surface, p);
    CHECK(s3::rs::abs(negative.force_world.x + 5.12f) < 0.00001f);
    CHECK(s3::rs::abs(negative.force_world.z + 3.84f) < 0.00001f);
    CHECK_EQ(negative.point_body.y, 0.12f);
}

S3_TEST(slide_yaw_past_ninety_degrees_uses_signed_ground_axis) {
    const SlideSettings s = settings();
    SlideInput p = input();
    p.velocity = {0.f, 0.f, 4.f, 0.f};
    p.angular_velocity = {9.f, -2.f, 5.f, 0.f};
    p.slide = 0.75f;
    const SlideSurface surface{curve(0.f), 2.f, 0.3f};
    const auto positive = angular_correction(s, surface, p);
    CHECK(s3::rs::abs(positive[1] - 0.14f) < 0.00001f);
    CHECK_EQ(positive[0], 0.f);
    CHECK_EQ(positive[2], 0.f);
    p.slide = -0.75f;
    const auto negative = angular_correction(s, surface, p);
    CHECK(s3::rs::abs(negative[1] + 0.26f) < 0.00001f);
}
}  // namespace
