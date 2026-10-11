// Port of skate-core/src/player/offboard/movement_intent/tests.rs.
#include "harness.hpp"
#include "s3/core/player/offboard/movement_intent.hpp"
#include "s3/core/player/offboard/movement_intent/math.hpp"

namespace {
using namespace s3::player::offboard::movement_intent;
using namespace s3::player::offboard::movement_intent::math;

template <std::size_t N>
PointGraph<N> constant(float value) {
    PointGraph<N> g;
    for (std::size_t i = 0; i < N; ++i) g.x[i] = float(i);
    g.y.fill(value);
    return g;
}
Settings settings() { return Settings{constant<4>(8.0f), constant<16>(4.0f), constant<8>(1.0f), 2.0f, constant<8>(0.0f)}; }
Input input() {
    Input i;
    i.magnitude = 1.0f;
    i.direction = {0.0f, 0.0f, 1.0f, 0.0f};
    i.edge_tangent = {0.0f, 0.0f, 1.0f, 0.0f};
    i.right = {1.0f, 0.0f, 0.0f, 0.0f};
    i.up = {0.0f, 1.0f, 0.0f, 0.0f};
    i.forward = {0.0f, 0.0f, 1.0f, 0.0f};
    i.obstacle_normal = {0.0f, 0.0f, -1.0f, 0.0f};
    return i;
}

S3_TEST(sprint_grace_survives_release_but_edge_disables_blend) {
    State state{};
    auto in = input();
    in.sprint_pressed = true;
    state.update(settings(), in);
    CHECK_EQ(state.speed, 8.0f);
    in.sprint_pressed = false;
    state.update(settings(), in);
    CHECK_EQ(state.speed, 8.0f);
    CHECK(state.sprint_grace > 0.0f);
    in.edge_active = true;
    state.update(settings(), in);
    CHECK_EQ(state.speed, 4.0f);
    CHECK(state.sprint_time > 0.0f);
}

S3_TEST(centered_obstacle_stops_speed_and_has_priority_over_slide) {
    State state{};
    auto in = input();
    in.obstacle = true;
    in.sliding = true;
    in.slide_velocity = {0.0f, 0.0f, 3.0f, 0.0f};
    state.update(settings(), in);
    CHECK(state.obstacle_centered);
    CHECK_EQ(state.speed, 0.0f);
    in.ignore_obstacle = true;
    state.update(settings(), in);
    CHECK(!state.obstacle_centered);
    CHECK(std::fabs(state.speed - 7.0f) < 0.00001f);
}

S3_TEST(steering_merge_replaces_opposition_and_preserves_stronger_same_sign) {
    State state{};
    state.steering = 0.8f;
    state.merge_steering(0.2f);
    CHECK_EQ(state.steering, 0.8f);
    state.merge_steering(-0.1f);
    CHECK_EQ(state.steering, -0.1f);
    state.merge_steering(-0.6f);
    CHECK_EQ(state.steering, -0.6f);
}

S3_TEST(edge_tangent_flips_before_target_and_flag_resets_next_tick) {
    State state{};
    auto in = input();
    in.edge_active = true;
    in.edge_tangent = {0.0f, 0.0f, -1.0f, 0.0f};
    in.edge_point = {0.2f, 0.0f, 1.0f, 0.0f};
    state.update(settings(), in);
    CHECK_EQ(state.edge_target, (Vector{0.2f, 0.0f, 1.5f, 0.0f}));
    CHECK(state.edge_aligned);
    CHECK(state.steering > 0.0f);
    in.edge_active = false;
    state.update(settings(), in);
    CHECK(!state.edge_aligned);
    CHECK_EQ(state.edge_target, (Vector{0.2f, 0.0f, 1.5f, 0.0f}));
}

S3_TEST(angle_degeneracy_and_half_turn_boundary_are_retained) {
    CHECK_EQ(signed_angle(Vector{}, {1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}), 0.0f);
    CHECK_EQ(normalize_or(Vector{}, {0.0f, 0.0f, 1.0f, 0.0f}), (Vector{0.0f, 0.0f, 1.0f, 0.0f}));
    const float pi = s3::rs::from_bits(0x40490fdbu);
    CHECK(std::fabs(wrap_angle(pi) - pi) < 0.000001f);
    CHECK(wrap_angle(pi + 0.1f) < 0.0f);
}

S3_TEST(edge_target_preserves_native_fourth_lane) {
    State state{};
    auto in = input();
    in.edge_active = true;
    in.edge_tangent[3] = 2.0f;
    in.edge_point[3] = 3.0f;
    state.update(settings(), in);
    CHECK_EQ(state.edge_target[3], 4.0f);
    CHECK_EQ(horizontal({1.0f, 4.0f, 0.0f, 9.0f}), (Vector{1.0f, 0.0f, 0.0f, 1.0f}));
}
}  // namespace
