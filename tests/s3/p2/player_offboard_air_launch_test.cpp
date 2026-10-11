// Port of skate-core/src/player/offboard/air_launch/tests.rs.
#include "harness.hpp"
#include "s3/core/player/offboard/air_launch.hpp"
#include "s3/core/trigonometry.hpp"

namespace {
using namespace s3::player::offboard::air_launch;
using namespace s3::player::offboard::air_launch::math;
namespace controller = s3::player::offboard::controller;
using s3::rs::to_bits;

Processed processed() {
    Processed p;
    p.board_position_112 = {7.0f, 8.0f, 9.0f, 2.0f};
    p.forward_224 = {0.0f, 0.0f, 1.0f, 0.0f};
    p.up_544 = UP;
    p.position_592 = {10.0f, 20.0f, 30.0f, 1.0f};
    p.velocity_608 = {};
    p.velocity_912 = {};
    p.departure_geometry = std::nullopt;
    p.previous_state_2504 = 500;
    p.current_state_2508 = 500;
    p.current_category_2512 = 500;
    p.previous_category_2516 = 500;
    return p;
}
PointGraph<8> curve() {
    PointGraph<8> g;
    g.x = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f};
    g.y.fill(90.0f);
    return g;
}
Settings settings() { return Settings{0.8f, 0.9f}; }
controller::State fresh() { return controller::State::new_({std::nullopt, std::nullopt, std::nullopt}); }

S3_TEST(mode4_turn_clamp_reads_angular_velocity_not_edge_position) {
    auto p = processed();
    p.flags_2476 = 0x80000;
    p.velocity_912 = {0.0f, 0.0f, 4.0f, 0.0f};
    auto state = fresh();
    state.motion.angular_velocity_688 = 4.0f;
    state.intent.edge_target[0] = -100.0f;
    auto first = Packet::initialized(0.0f);
    produce(first, state, curve(), settings(), p, true).unwrap();
    state.intent.edge_target[0] = 100.0f;
    auto second = Packet::initialized(0.0f);
    produce(second, state, curve(), settings(), p, true).unwrap();
    CHECK_EQ(first.velocity_0, second.velocity_0);
    state.motion.angular_velocity_688 = -4.0f;
    auto opposite = Packet::initialized(0.0f);
    produce(opposite, state, curve(), settings(), p, true).unwrap();
    CHECK_NE(second.velocity_0, opposite.velocity_0);
}

Packet run(const Processed& p) {
    auto packet = Packet::initialized(s3::rs::f32::NAN_);
    produce(packet, fresh(), curve(), settings(), p, true).unwrap();
    return packet;
}
void near(float a, float b) { CHECK_MSG(s3::rs::abs(a - b) < 2.0e-5f, a << " != " << b); }
void vector(Vector a, Vector b) {
    for (int i = 0; i < 4; ++i) near(a[i], b[i]);
}

S3_TEST(all_seven_dispatch_modes_and_previous_state_selector) {
    auto p = processed();
    const std::pair<std::uint32_t, int> cases[] = {{100, 0}, {400, 1}, {200, 2}, {700, 6}};
    for (auto [category, expected] : cases) {
        p.current_category_2512 = category;
        CHECK_EQ(int(mode(p, true)), expected);
    }
    p.current_category_2512 = 500;
    CHECK_EQ(int(mode(p, true)), 5);
    p.flags_2476 = 0x80000;
    CHECK_EQ(int(mode(p, true)), 4);
    p.flags_2480 = 0x80;
    CHECK_EQ(int(mode(p, true)), 3);
    p.flags_2480 = 0;
    p.current_state_2508 = 503;
    CHECK_EQ(int(mode(p, true)), 3);
    p.previous_category_2516 = 200;
    CHECK_EQ(int(mode(p, false)), 2);
}

S3_TEST(initializer_preserves_only_unwritten_scalar104) {
    const auto p = Packet::initialized(123.0f);
    CHECK_EQ(p.scalar_104, 123.0f);
    CHECK_EQ(p.up_48, UP);
    CHECK_EQ(p.board_position_80, ZERO);
    CHECK(!p.flag_117);
}

S3_TEST(passive_modes_publish_common_fields_and_preserve_caller_fields) {
    for (std::uint32_t category : {200u, 700u}) {
        auto p = processed();
        p.current_category_2512 = category;
        p.velocity_608 = {1.0f, 2.0f, 3.0f, 4.0f};
        p.forward_224 = {0.0f, 99.0f, 2.0f, 6.0f};
        auto packet = Packet::initialized(s3::rs::f32::NAN_);
        packet.board_position_80 = {4.0f, 4.0f, 4.0f, 4.0f};
        packet.has_board_position_116 = true;
        packet.flag_117 = true;
        produce(packet, fresh(), curve(), settings(), p, true).unwrap();
        CHECK_EQ(packet.velocity_0, p.velocity_608);
        CHECK_EQ(packet.secondary_velocity_16, p.velocity_608);
        vector(packet.forward_64, {0.0f, 0.0f, 1.0f, 3.0f});
        CHECK_EQ(packet.position_32, p.position_592);
        CHECK_EQ(to_bits(packet.scalar_104), 0x3f32b8c2u);
        CHECK_EQ(packet.board_position_80, (Vector{4.0f, 4.0f, 4.0f, 4.0f}));
        CHECK(packet.has_board_position_116 && packet.flag_117);
    }
}

S3_TEST(riding_launch_boost_is_not_a_clamp_and_advances_all_lanes) {
    const std::pair<float, float> cases[] = {{-10.0f, -7.0f}, {-2.0f, 1.0f}, {0.0f, 3.0f},
                                             {2.0f, 3.0f},    {3.0f, 3.0f},  {8.0f, 8.0f}};
    for (auto [y, want] : cases) {
        auto p = processed();
        p.current_category_2512 = 100;
        p.velocity_608 = {2.0f, y, 4.0f, 6.0f};
        const auto packet = run(p);
        CHECK_EQ(packet.velocity_0[1], want);
        CHECK_EQ(packet.secondary_velocity_16, p.velocity_608);
        CHECK_EQ(packet.board_position_80, p.board_position_112);
        CHECK(packet.has_board_position_116);
        near(packet.position_32[1], s3::rs::mul_add(want, DT, 20.0f));
        near(packet.position_32[3], s3::rs::mul_add(6.0f, DT, 1.0f));
    }
}

S3_TEST(mode1_projects_away_from_supplied_axis_before_side_impulse) {
    auto p = processed();
    p.current_category_2512 = 400;
    p.position_592 = {3.0f, 50.0f, 4.0f, 0.0f};
    p.departure_geometry = DepartureGeometry{{}, UP};
    const auto packet = run(p);
    vector(packet.velocity_0, {1.2f, 1.0f, 1.6f, 0.0f});
    CHECK_EQ(packet.kind_108, 6u);
    vector(packet.secondary_velocity_16, ZERO);
}

S3_TEST(mode3_sideways_sign_and_stationary_guard) {
    auto p = processed();
    p.current_state_2508 = 503;
    CHECK_EQ(run(p).velocity_0, ZERO);
    p.velocity_608 = {0.0f, 1.0f, 2.0f, 0.0f};
    near(run(p).velocity_0[0], -0.75f);
    p.flags_2476 = 4;
    near(run(p).velocity_0[0], 0.75f);
    CHECK_EQ(run(p).position_32, p.position_592);
}

S3_TEST(mode5_speeds_either_side_and_fast_vertical_caps) {
    auto p = processed();
    p.velocity_608 = {0.0f, 0.0f, 1.874f, 0.0f};
    p.velocity_912 = {4.0f, 20.0f, 8.0f, 12.0f};
    const auto slow = run(p);
    vector(slow.velocity_0, {0.0f, 1.0f, 2.5f, 0.0f});
    CHECK_EQ(to_bits(slow.scalar_96), 0x3f060a92u);
    p.velocity_608[2] = 1.876f;
    const auto fast = run(p);
    vector(fast.velocity_0, {3.0f, 5.0f, 6.0f, 9.0f});
    CHECK_EQ(to_bits(fast.scalar_96), 0x3db2b8c2u);
    p.velocity_912[1] = -20.0f;
    CHECK_EQ(run(p).velocity_0[1], -10.0f);
}

S3_TEST(mode5_pc_refinement_at_raw_1875_is_below_comparison_threshold) {
    auto p = processed();
    p.velocity_608 = {0.0f, 0.0f, 1.875f, 0.0f};
    p.velocity_912 = {4.0f, 20.0f, 8.0f, 12.0f};
    CHECK_EQ(to_bits(length(p.velocity_608)), 0x3fefffffu);
    const auto packet = run(p);
    vector(packet.velocity_0, {0.0f, 1.0f, 2.5f, 0.0f});
    CHECK_EQ(to_bits(packet.scalar_96), 0x3f060a92u);
}

S3_TEST(mode5_stationary_uses_flattened_forward_including_w) {
    auto p = processed();
    p.forward_224 = {0.0f, 7.0f, 2.0f, 4.0f};
    vector(run(p).velocity_0, {0.0f, 1.0f, 2.5f, 5.0f});
}

S3_TEST(jump_stationary_control_and_impulse_use_separate_stock_scalars) {
    auto p = processed();
    p.flags_2476 = 0x80000;
    p.raw_x_2692 = 0.6f;
    p.raw_z_2688 = 0.8f;
    const auto packet = run(p);
    near(packet.velocity_0[0], 0.48f);
    near(packet.velocity_0[2], 0.64f);
    near(packet.velocity_0[1], s3::rs::sqrt(0.9f * 19.6f));
    CHECK_EQ(packet.velocity_0[3], 0.0f);
    vector(packet.secondary_velocity_16, {0.6f, 0.0f, 0.8f, 0.0f});
    CHECK_EQ(packet.kind_108, 6u);
    CHECK_EQ(packet.kind_112, 3u);
}

S3_TEST(jump_processed_gate_suppresses_stationary_stick_impulse) {
    auto p = processed();
    p.flags_2476 = 0x80000;
    p.flags_2472 = 0x10000000;
    p.raw_x_2692 = 1.0f;
    const auto packet = run(p);
    CHECK_EQ(packet.velocity_0[0], 0.0f);
    CHECK_EQ(packet.secondary_velocity_16, ZERO);
}

S3_TEST(jump_backward_velocity_is_removed_before_control) {
    auto p = processed();
    p.flags_2476 = 0x80000;
    p.flags_2472 = 0x10000000;
    p.velocity_912 = {0.0f, -9.0f, -3.0f, 0.0f};
    const auto packet = run(p);
    near(packet.velocity_0[2], 0.0f);
    CHECK_EQ(packet.secondary_velocity_16, ZERO);
}

S3_TEST(obstacle_positive_sixty_degree_branch_keeps_raw_45_argument) {
    auto p = processed();
    p.flags_2476 = 0x80000;
    p.velocity_912 = {0.0f, 0.0f, 2.0f, 0.0f};
    p.raw_x_2692 = -0.8660254f;
    p.raw_z_2688 = 0.5f;
    auto state = fresh();
    state.contact.active = true;
    auto packet = Packet::initialized(0.0f);
    produce(packet, state, curve(), settings(), p, true).unwrap();
    near(packet.secondary_velocity_16[0], -0.8660254f);
    near(packet.secondary_velocity_16[2], 0.5f);
    near(packet.velocity_0[0], 0.0f);
    near(packet.velocity_0[2], 1.6f);
}

S3_TEST(obstacle_signed_negative_angle_and_over_ninety_branch) {
    auto p = processed();
    p.flags_2476 = 0x80000;
    p.velocity_912 = {0.0f, 0.0f, 2.0f, 0.0f};
    auto state = fresh();
    state.contact.active = true;
    auto packet = Packet::initialized(0.0f);
    p.raw_x_2692 = 1.0f;
    produce(packet, state, curve(), settings(), p, true).unwrap();
    vector(packet.secondary_velocity_16, {1.0f, 0.0f, 0.0f, 0.0f});
    p.raw_x_2692 = -1.0f;
    p.raw_z_2688 = -0.1f;
    produce(packet, state, curve(), settings(), p, true).unwrap();
    vector(packet.secondary_velocity_16, {0.0f, 0.0f, 1.0f, 0.0f});
}

S3_TEST(jump_slope_frame_reduces_vertical_impulse_by_up_y) {
    auto p = processed();
    p.flags_2476 = 0x80000;
    p.flags_2472 = 0x10000000;
    auto state = fresh();
    state.frame_output.frame[1] = {0.6f, 0.8f, 0.0f, 0.0f};
    auto packet = Packet::initialized(0.0f);
    produce(packet, state, curve(), settings(), p, true).unwrap();
    const float lift = s3::rs::sqrt(0.9f * 19.6f) * 0.8f;
    near(packet.velocity_0[0], lift * 0.6f);
    near(packet.velocity_0[1], lift * 0.8f);
    CHECK_EQ(packet.up_48, UP);
}

S3_TEST(steering_uses_live760_then_angular_velocity_window_and_final_angle_cap) {
    auto p = processed();
    p.flags_2476 = 0x80000;
    p.flags_2472 = 0x10000000;
    p.velocity_912 = {0.0f, 0.0f, 2.0f, 0.0f};
    auto state = fresh();
    state.intent.steering = 1.0f;
    state.intent.original_steering = -1.0f;
    auto packet = Packet::initialized(0.0f);
    produce(packet, state, curve(), settings(), p, true).unwrap();
    const auto [sin, cos] = s3::trigonometry::sin_cos(s3::rs::from_bits(0x3e860a92u));
    near(packet.velocity_0[0], 1.6f * sin);
    near(packet.velocity_0[2], 1.6f * cos);
    state.motion.angular_velocity_688 = -3.0f;
    produce(packet, state, curve(), settings(), p, true).unwrap();
    CHECK(packet.velocity_0[0] < 0.0f);
}

S3_TEST(missing_departure_geometry_fails_before_any_packet_write) {
    auto p = processed();
    p.previous_category_2516 = 400;
    auto packet = Packet::initialized(123.0f);
    packet.velocity_0 = {9.0f, 9.0f, 9.0f, 9.0f};
    packet.position_32 = {8.0f, 8.0f, 8.0f, 8.0f};
    const auto before = packet;
    const auto result = produce(packet, fresh(), curve(), settings(), p, false);
    CHECK(result.is_err());
    CHECK_EQ(std::string(result.unwrap_err()),
             std::string("Offboard launch mode1 requires source-backed departure geometry1120/1136"));
    CHECK(packet == before);
}

S3_TEST(absent_departure_geometry_is_valid_outside_selected_mode1) {
    auto p = processed();
    p.previous_category_2516 = 400;
    p.current_category_2512 = 200;
    p.velocity_608 = {1.0f, 2.0f, 3.0f, 4.0f};
    auto packet = Packet::initialized(0.0f);
    produce(packet, fresh(), curve(), settings(), p, true).unwrap();
    CHECK_EQ(packet.velocity_0, p.velocity_608);
}

S3_TEST(geometric_launch_rotation_does_not_turn_x_into_a_fourth_velocity) {
    const Vector v{-2.0225198f, 4.1746917f, 0.7100913f, 0.0f};
    const auto rotated0 = rotate(v, {0.0f, 1.0f, 0.0f, 0.0f}, 0.0f);
    CHECK_EQ(rotated0, v);
    for (float angle : {-0.2f, 0.2f}) {
        const auto rotated = rotate(v, {0.0f, 1.0f, 0.0f, 0.0f}, angle);
        CHECK_EQ(rotated[3], 0.0f);
        const auto [s, c] = s3::trigonometry::sin_cos(angle);
        CHECK(s3::rs::abs(rotated[0] - (v[0] * c + v[2] * s)) < 1e-6f);
        CHECK_EQ(rotated[1], v[1]);
        CHECK(s3::rs::abs(rotated[2] - (-v[0] * s + v[2] * c)) < 1e-6f);
    }
}
}  // namespace
