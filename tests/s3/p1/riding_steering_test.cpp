// Port of skate-core/src/riding/tests/steering.rs.
#include "harness.hpp"
#include "s3/core/riding/steering.hpp"

namespace {
using namespace s3::riding::steering;
using s3::point_graph::PointGraph;

SteeringSettings settings() {
    SteeringSettings s;
    s.hard_turn_increase = 1.0f;
    s.damping = 0.7f;
    s.speed_graph_max_speed = 50.0f;
    s.push_scalar_increment = 0.05f;
    s.push_scalar_decrement = 0.015f;
    s.push_scalar_min = 0.5f;
    s.manual_scalar = 0.25f;
    s.general_scalar = 0.6f;
    s.tight_trucks_scalar = 0.7f;
    s.tilt_blending = 0.2f;
    s.speed_graph = PointGraph<8>{{0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 1.0f},
                                  {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}};
    s.input_graph = PointGraph<8>{{0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 1.0f},
                                  {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}};
    return s;
}

SteeringInput input() {
    SteeringInput i;
    i.turn = 0.5f;
    i.flipped_controls_scalar = 1.0f;
    return i;
}

S3_TEST(hard_turn_uses_turn_sign_including_zero_not_hard_turn_sign) {
    const auto s = settings();
    auto i = input();
    i.hard_turn = -1.0f;
    CHECK_EQ(calculate_tilt(s, i, nullptr, nullptr), 1.2f);
    i.turn = -0.5f;
    CHECK_EQ(calculate_tilt(s, i, nullptr, nullptr), -1.2f);
    i.turn = -0.0f;
    CHECK_EQ(calculate_tilt(s, i, nullptr, nullptr), 1.2f);
}

S3_TEST(optional_state_pointers_and_pushing_recovery_preserve_native_limits) {
    const auto s = settings();
    float scalar = 1.0f;
    float turn = 0.0f;
    auto i = input();
    i.pushing = true;
    calculate_tilt(s, i, &scalar, &turn);
    CHECK_EQ(turn, 0.35f);
    CHECK_EQ(scalar, 0.985f);
    for (int n = 0; n < 100; ++n) {
        calculate_tilt(s, i, &scalar, nullptr);
    }
    CHECK_EQ(scalar, 0.5f);
    i.pushing = false;
    for (int n = 0; n < 100; ++n) {
        calculate_tilt(s, i, &scalar, nullptr);
    }
    CHECK_EQ(scalar, 1.0f);
}

S3_TEST(manual_tightness_and_flipped_controls_multiply_the_tilt) {
    const auto s = settings();
    const float base = calculate_tilt(s, input(), nullptr, nullptr);
    auto i = input();
    i.balance = 1.0f;
    i.truck_tightness = 1.0f;
    i.flipped_controls_scalar = -1.0f;
    CHECK(s3::rs::abs(calculate_tilt(s, i, nullptr, nullptr) - base * -0.25f * 0.7f) < 1e-7f);
}

S3_TEST(truck_contact_flags_swap_with_stance_and_first_inactive_tick_holds_angle) {
    TruckSteeringState regular{};
    regular.update(0.6f, 0.2f, 0, 1u << 27);
    CHECK(regular.targets[0] > 0.0f);
    CHECK_EQ(regular.targets[1], 0.0f);
    TruckSteeringState switch_{};
    switch_.update(0.6f, 0.2f, 1u << 20, 1u << 27);
    CHECK_EQ(regular.targets, (std::array<float, 2>{switch_.targets[1], switch_.targets[0]}));
    const float previous = regular.targets[0];
    regular.update(0.6f, 0.2f, 0, 0);
    CHECK_EQ(regular.targets[0], previous);
    regular.update(0.6f, 0.2f, 0, 0);
    CHECK_EQ(regular.targets[0], previous * 0.983f);
}

S3_TEST(truck_reactivation_completes_the_native_ramp) {
    TruckSteeringState state{};
    for (int n = 0; n < 11; ++n) {
        state.update(0.6f, 1.0f, 0, 3u << 26);
    }
    CHECK_EQ(state.targets, (std::array<float, 2>{0.6f, 0.6f}));
    CHECK(state.activation_time[0] > 0.165f);
}

}  // namespace
