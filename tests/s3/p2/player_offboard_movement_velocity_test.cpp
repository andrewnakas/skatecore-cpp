// Port of skate-core/src/player/offboard/movement_velocity/tests.rs.
#include <cmath>

#include "harness.hpp"
#include "s3/core/player/offboard/movement_velocity.hpp"
#include "s3/core/player/offboard/movement_velocity/math.hpp"

namespace {
using namespace s3::player::offboard::movement_velocity;
using namespace s3::player::offboard::movement_velocity::math;
bool is_subnormal(float v) { return std::fpclassify(v) == FP_SUBNORMAL; }

PointGraph<8> curve(float value) {
    PointGraph<8> g;
    for (std::size_t i = 0; i < 8; ++i) g.x[i] = float(i);
    g.y.fill(value);
    return g;
}
Settings settings() { return Settings{curve(1.0f), curve(3.0f), curve(90.0f), curve(15.0f)}; }
Input input() {
    return Input{{0.0f, 0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, 5.0f, 1.0f, false, false,
                 {0.0f, 0.0f, -1.0f, 0.0f}, -1.0f, 1.0f, Vector{}, 0};
}

S3_TEST(tiny_velocity_delta_remains_finite_and_reaches_target) {
    const Vector current{-1.6481168f, 1.0e-21f, 5.7692037f, 0.0f};
    const Vector target{-1.6481168f, 0.0f, 5.7692037f, 0.0f};
    const Vector delta = sub(target, current);
    CHECK(is_subnormal(dot(delta, delta)));
    const auto result = limit_delta(target, current, 0.2f);
    CHECK_EQ(result, target);
    CHECK(std::isfinite(length(delta)));
    CHECK(std::fabs(length(delta) / 1.0e-21f - 1.0f) < 0.001f);
}

S3_TEST(tiny_velocity_refinement_boundaries_preserve_limits_and_invalid_inputs) {
    for (int exponent = -74; exponent <= -60; ++exponent) {
        const float component = s3::rs::powi(2.0f, exponent);
        const Vector delta{component, 0.0f, 0.0f, 0.0f};
        const float measured = length(delta);
        CHECK(std::isfinite(measured) && measured > 0.0f);
        CHECK(std::fabs(measured / component - 1.0f) < 1e-6f);
        const float maximum = component * 0.5f;
        const auto limited = limit_delta(delta, Vector{}, maximum);
        for (float v : limited) CHECK(std::isfinite(v));
        CHECK(std::fabs(limited[0] / maximum - 1.0f) < 1e-6f);
    }
    CHECK_EQ(length(Vector{}), 0.0f);
    CHECK(std::isnan(length({s3::rs::f32::NAN_, 0.0f, 0.0f, 0.0f})));
    CHECK(std::isnan(limit_delta({s3::rs::f32::NAN_, 0.0f, 0.0f, 0.0f}, Vector{}, 0.2f)[0]));
}

S3_TEST(tiny_velocity_correction_does_not_poison_walking_state) {
    State state{};
    state.velocity = {0.0f, 1.0e-21f, 5.0f, 0.0f};
    state.speed = 5.0f;
    auto in = input();
    in.steering = 0.0f;
    for (int k = 0; k < 120; ++k) {
        state.update(settings(), in);
        CHECK_EQ(state.velocity, (Vector{0.0f, 0.0f, 5.0f, 0.0f}));
        CHECK_EQ(state.speed, 5.0f);
        CHECK(std::isfinite(state.forward_delta) && std::isfinite(state.right_delta));
    }
}

S3_TEST(desired_forward_delta_differs_from_limited_velocity) {
    State state{};
    state.update(settings(), input());
    CHECK_EQ(state.forward_delta, 5.0f);
    CHECK(std::fabs(state.velocity[2] - 0.2f) < 0.000001f);
    CHECK_EQ(state.right_delta, 0.0f);
    CHECK_EQ(state.turn, 15.0f * s3::rs::from_bits(0x3c8efa35u));
    CHECK_EQ(state.override_remaining, -1.0f);
}

S3_TEST(slope_mode_uses_its_own_speed_and_ignores_override) {
    State state{};
    auto in = input();
    in.slope_mode = true;
    in.override_gate = 0.0f;
    in.override_velocity = {20.0f, 0.0f, 0.0f, 0.0f};
    state.update(settings(), in);
    CHECK_EQ(state.forward_delta, 3.0f);
    CHECK(std::fabs(state.speed - 0.5f) < 0.000001f);
    CHECK_EQ(state.override_remaining, -1.0f);
}

S3_TEST(override_first_tick_retains_old_velocity_then_fast_target_reduces_blend) {
    State state{};
    auto in = input();
    in.override_gate = 0.0f;
    in.override_velocity = {0.0f, 0.0f, 10.0f, 0.0f};
    state.update(settings(), in);
    CHECK_EQ(state.velocity, Vector{});
    CHECK(state.override_remaining < 1.0f);
    state.update(settings(), in);
    CHECK_EQ(state.velocity, (Vector{0.0f, 0.0f, 5.0f, 0.0f}));
}

S3_TEST(minimum_motion_override_uses_projected_forward) {
    State state{};
    auto in = input();
    in.override_gate = 0.0f;
    in.flags = 8;
    state.update(settings(), in);
    CHECK_EQ(state.velocity, (Vector{0.0f, 0.0f, 1.0f, 0.0f}));
}

S3_TEST(obstacle_projects_desired_and_limited_velocity_independently) {
    State state{};
    auto in = input();
    in.obstacle = true;
    state.update(settings(), in);
    CHECK_EQ(state.velocity, Vector{});
    CHECK_EQ(state.forward_delta, 0.0f);
}

S3_TEST(nonunit_projection_subtracts_original_direction_and_zero_is_safe) {
    CHECK_EQ(remove_positive({2.0f, 0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f, 0.0f}), (Vector{-2.0f, 0.0f, 0.0f, 0.0f}));
    CHECK_EQ(remove_positive({-2.0f, 0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f, 0.0f}), (Vector{-2.0f, 0.0f, 0.0f, 0.0f}));
    CHECK_EQ(remove_positive({2.0f, 0.0f, 0.0f, 0.0f}, Vector{}), (Vector{2.0f, 0.0f, 0.0f, 0.0f}));
    CHECK_EQ(limit_delta(Vector{}, Vector{}, 0.2f), Vector{});
}

S3_TEST(upward_slowdown_retains_gravity_even_beyond_two_per_second_bound) {
    State state{};
    state.velocity = {0.0f, 6.0f, 8.0f, 0.0f};
    state.speed = 10.0f;
    auto in = input();
    in.forward = {0.0f, 0.6f, 0.8f, 0.0f};
    in.plane_normal = {1.0f, 0.0f, 0.0f, 0.0f};
    in.desired_speed = 1.0f;
    state.update(settings(), in);
    const float expected = 10.0f + (s3::rs::from_bits(0xc11ccccdu) * s3::rs::from_bits(0x3c888889u)) * 0.6f;
    CHECK(std::fabs(state.speed - expected) < 0.00001f);
    CHECK(state.speed < 10.0f - 2.0f * s3::rs::from_bits(0x3c888889u));
}

S3_TEST(vector_delta_limits_xyz_and_preserves_scaled_w) {
    State state{};
    auto in = input();
    in.forward[3] = 2.0f;
    state.update(settings(), in);
    CHECK(std::fabs(state.speed - 0.2f) < 0.000001f);
    CHECK(std::fabs(state.velocity[3] - 0.4f) < 0.000001f);
    CHECK_EQ(remove_positive({2.0f, 0.0f, 0.0f, 7.0f}, {2.0f, 0.0f, 0.0f, 3.0f}), (Vector{-2.0f, 0.0f, 0.0f, 1.0f}));
}
}  // namespace
