// Port of skate-core/src/air/known/tests/mod.rs.
#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "air_known_mock.hpp"
#include "harness.hpp"

namespace {

using namespace s3::air::known;
using air_known_mock::frame;
using air_known_mock::mode;
using air_known_mock::MockRuntime;
using air_known_mock::output;
using air_known_mock::reckoning;
using air_known_mock::settings;
using air_known_mock::state;
namespace mock = air_known_mock;
namespace rs = s3::rs;
using V4 = std::array<float, 4>;

void assert_close(float actual, float expected) {
    CHECK_MSG(rs::abs(actual - expected) < 0.00001f, actual << " != " << expected);
}

std::size_t position(const std::vector<std::string>& calls, const std::string& value) {
    const auto it = std::find(calls.begin(), calls.end(), value);
    CHECK_MSG(it != calls.end(), "missing call " << value);
    return static_cast<std::size_t>(it - calls.begin());
}

bool any(const std::vector<std::string>& calls, const std::string& value) {
    return std::find(calls.begin(), calls.end(), value) != calls.end();
}

S3_TEST(init_trajectory_info_builds_stock_landing_values) {
    KnownAirState st = state();
    KnownAirFrame fr = frame();
    fr.selector_landing_normal_2656 = {0.0f, 1.0f, 0.0f, 0.0f};
    KnownAirSettings se = settings();
    se.flip_scalar = 0.5f;
    se.min_target_heading_velocity_420 = 0.5f;
    const KnownAirModeSettings mo = mode();
    MockRuntime runtime = MockRuntime::new_();
    runtime.prediction.trajectory.velocity = {3.0f, 4.0f, 0.0f, 0.0f};
    runtime.prediction.trajectory.scalar_48 = 9.0f;  // Independent query horizon.
    runtime.prediction.collision_time_48 = 2.0f;
    runtime.prediction.collision_frame_128 = 3;
    runtime.contact = {9.0f, 8.0f, 7.0f, 0.0f};
    runtime.selector_vector = {6.0f, 5.0f, 4.0f, 0.0f};
    runtime.highest = {V4{1.0f, 12.0f, 3.0f, 0.0f}, 0.75f};

    init_trajectory_info(st, fr, se, mo, runtime);

    CHECK_EQ(st.trajectory_apex_96, runtime.highest.first);
    CHECK_EQ(st.time_to_apex_200, 0.75f);
    CHECK_EQ(st.collision_position_112, runtime.contact);
    CHECK_EQ(st.selector_vector_128, runtime.selector_vector);
    assert_close(st.collision_normal_speed_176, 4.0f);
    CHECK_EQ(st.landing_heading_80, (V4{1.0f, 0.0f, 0.0f, 0.0f}));
    CHECK(st.landing_heading_valid_209);
    assert_close(st.body_flip_target_speed_204, -rs::f32::consts::FRAC_PI_2);
}

S3_TEST(init_negative_collision_clears_speed_and_heading_validity) {
    KnownAirState st = state();
    st.collision_normal_speed_176 = 99.0f;
    st.landing_heading_valid_209 = true;
    const KnownAirFrame fr = frame();
    const KnownAirSettings se = settings();
    const KnownAirModeSettings mo = mode();
    MockRuntime runtime = MockRuntime::new_();
    runtime.prediction.collision_time_48 = -1.0f;

    init_trajectory_info(st, fr, se, mo, runtime);

    CHECK_EQ(st.collision_normal_speed_176, 0.0f);
    CHECK_EQ(st.landing_heading_80, (V4{0.0f, 0.0f, 0.0f, 0.0f}));
    CHECK(!st.landing_heading_valid_209);
}

S3_TEST(enter_preserves_recovered_effect_order_and_initializes_heights) {
    KnownAirState st = state();
    KnownAirFrame fr = frame();
    fr.start_height_484 = 3.0f;
    fr.velocity_400 = {-1.0f, 0.0f, 0.0f, 0.0f};
    fr.start_flip_reference_96 = {1.0f, 0.0f, 0.0f, 0.0f};
    const KnownAirSettings se = settings();
    const KnownAirModeSettings mo = mode();
    MockRuntime runtime = MockRuntime::new_();
    runtime.board_y = 4.0f;
    runtime.com = {0.0f, 5.0f, 0.0f, 0.0f};
    runtime.targeting_grind = true;
    runtime.closest = {11, V4{0.25f, 0.0f, 0.0f, 0.0f}};

    enter(st, fr, se, mo, runtime);

    CHECK_EQ(st.start_y_184, 3.0f);
    CHECK_EQ(st.max_y_188, 4.0f);
    CHECK_EQ(st.com_max_y_192, 5.0f);
    CHECK_EQ(st.trajectory_index_216, 11);
    CHECK_EQ(st.trajectory_follow_offset_144, runtime.closest.second);
    CHECK(st.targeting_grind_213);
    CHECK(st.start_flipped_210);
    CHECK(runtime.calls.size() >= 9);
    CHECK_EQ((std::vector<std::string>(runtime.calls.begin(), runtime.calls.begin() + 9)),
             (std::vector<std::string>{
                 "board.angular_only",
                 "collision.enabled:true",
                 "collision.state:7",
                 "skeleton.truck_tilt:false",
                 "skeleton.ik:true",
                 "reckoning.reset_flip",
                 "footplant.flag:false",
                 "footplant.reset",
                 "board.height",
             }));
    CHECK(any(runtime.calls, "grind.start"));
}

S3_TEST(exit_restores_velocity_before_selector_and_grind_cleanup) {
    KnownAirState st = state();
    st.trajectory_index_216 = 0;
    KnownAirFrame fr = frame();
    fr.next_physics_state_2500 = 100;
    fr.ground_normal_464 = {0.0f, 1.0f, 0.0f, 0.0f};
    KnownAirSettings se = settings();
    se.landing_speed_scalar_vs_ground_normal_y_240 = mock::flat_graph(2.0f);
    KnownAirReckoningFields reck = reckoning();
    reck.body_spin_angle_1568 = 2.0f;
    reck.body_spin_speed_1572 = 3.0f;
    MockRuntime runtime = MockRuntime::new_();
    runtime.restore_geometry = RestoreVelocityGeometry{V4{2.0f, 0.0f, 0.0f, 0.0f}, 0.0f, 0.5f};
    runtime.board_velocity = {0.0f, 1.0f, 0.0f, 0.0f};
    runtime.forward = {1.0f, 0.0f, 0.0f, 0.0f};

    exit(st, fr, se, reck, runtime);

    CHECK_EQ(st.trajectory_index_216, 1);
    CHECK_EQ(runtime.written_board_velocity, (V4{3.0f, 1.0f, 0.0f, 0.0f}));
    CHECK_EQ(fr.forward_speed_2612, 3.0f);
    CHECK_EQ(reck.body_spin_speed_1572, 0.0f);
    CHECK_EQ(reck.body_spin_angle_1568, 0.0f);
    const std::size_t reset = position(runtime.calls, "reckoning.reset_flip");
    const std::size_t write = position(runtime.calls, "board.set_velocity");
    const std::size_t selector = position(runtime.calls, "selector.reset");
    const std::size_t grind = position(runtime.calls, "grind.started:false");
    CHECK(reset < write && write < selector && selector < grind);
}

S3_TEST(update_runs_footplant_grind_and_publication_in_native_order) {
    KnownAirState st = state();
    st.time_in_state_180 = 0.2f;
    st.collision_time_196 = 0.25f;
    st.reached_apex_208 = true;
    st.targeting_grind_213 = true;
    st.trajectory_index_216 = 4;
    KnownAirFrame fr = frame();
    fr.flags_2480 = 0x04000000u;
    fr.delta_time_2604 = 1.0f / 60.0f;
    const KnownAirSettings se = settings();
    const KnownAirModeSettings mo = mode();
    const KnownAirReckoningFields reck = reckoning();
    MockRuntime runtime = MockRuntime::new_();
    runtime.grind_started = true;
    runtime.grind_adjusting = true;
    runtime.board_y = 7.0f;
    runtime.com = {0.0f, 8.0f, 0.0f, 0.0f};

    update(st, fr, se, mo, reck, runtime);

    CHECK(st.grind_air_adjust_activated_212);
    CHECK_EQ(st.trajectory_index_216, 5);
    assert_close(st.time_in_state_180, 0.2f + fr.delta_time_2604);
    CHECK_EQ(st.max_y_188, 7.0f);
    CHECK_EQ(st.com_max_y_192, 8.0f);
    const std::vector<std::string> expected_bodies{"3152", "3156", "3160", "3168", "3172", "3176"};
    std::vector<std::string> actual_bodies;
    const std::string prefix = "grind.disable:";
    for (const auto& c : runtime.calls) {
        if (c.compare(0, prefix.size(), prefix) == 0) actual_bodies.push_back(c.substr(prefix.size()));
    }
    CHECK_EQ(actual_bodies, expected_bodies);
    const std::size_t foot = position(runtime.calls, "footplant.shift");
    const std::size_t collision = position(runtime.calls, "collision.update");
    const std::size_t reckon = position(runtime.calls, "reckoning.update_air");
    const std::size_t skeleton = position(runtime.calls, "skeleton.update_known_air");
    const std::size_t head = position(runtime.calls, "head.target:true");
    const std::size_t steering = position(runtime.calls, "board.steering_zero");
    CHECK(foot < collision && collision < reckon && reckon < skeleton && skeleton < head && head < steering);
}

S3_TEST(trajectory_follow_uses_selector_2704_and_honors_locked_index) {
    KnownAirState st = state();
    st.trajectory_index_216 = 0;
    st.time_in_state_180 = 2.0f;
    KnownAirFrame fr = frame();
    fr.flags_2468 = 0x00000008u;
    const KnownAirSettings se = settings();
    MockRuntime runtime = MockRuntime::new_();
    runtime.prediction.trajectory.position = {100.0f, 0.0f, 0.0f, 0.0f};
    runtime.selector_trajectory.position = {10.0f, 0.0f, 0.0f, 0.0f};

    update_trajectory_follow(st, fr, se, runtime);

    CHECK_EQ(st.trajectory_index_216, 0);
    CHECK_EQ(st.target_com_position_160, (V4{10.0f, 0.0f, 0.0f, 0.0f}));
}

S3_TEST(auto_spin_uses_recovered_alignment_chain_and_manual_gate) {
    KnownAirState st = state();
    st.landing_heading_valid_209 = true;
    st.landing_normal_64 = {0.0f, 1.0f, 0.0f, 0.0f};
    st.landing_heading_80 = {1.0f, 0.0f, 0.0f, 0.0f};
    st.collision_time_196 = 1.0f;
    KnownAirFrame fr = frame();
    fr.delta_time_2604 = 1.0f / 60.0f;
    fr.skater_up_544 = {0.0f, 1.0f, 0.0f, 0.0f};
    KnownAirSettings se = settings();
    se.max_heading_adjust_vs_up_y_160 = mock::flat_graph(180.0f);
    se.min_auto_body_speed_424 = 0.01f;
    const KnownAirModeSettings mo = mode();
    const KnownAirReckoningFields reck = reckoning();
    MockRuntime runtime = MockRuntime::new_();
    runtime.signed_angle = 0.5f;

    const float automatic = calculate_body_spin_speed(st, fr, se, mo, reck, runtime);
    assert_close(automatic, 0.6f);

    fr.body_spin_input_2640 = 1.0f;
    fr.flags_2472 = 0;
    runtime.calls.clear();
    const float masked_manual = calculate_body_spin_speed(st, fr, se, mo, reck, runtime);
    CHECK_EQ(masked_manual, 0.0f);
    CHECK(!any(runtime.calls, "math.signed_angle"));
}

S3_TEST(body_flip_starts_only_after_stock_flag_and_curve_gates) {
    KnownAirState st = state();
    st.collision_time_196 = 2.0f;
    st.body_flip_target_speed_204 = -3.0f;
    KnownAirFrame fr = frame();
    fr.flags_2468 = 0x20u | 0x80u;
    KnownAirSettings se = settings();
    se.flip_start_collision_time_vs_normal_y = mock::flat_graph(1.0f);
    const KnownAirModeSettings mo = mode();
    const KnownAirReckoningFields reck = reckoning();
    MockRuntime runtime = MockRuntime::new_();

    const float speed = calculate_body_flip_speed(st, fr, se, mo, reck, runtime);

    CHECK(st.body_flipping_211);
    CHECK_EQ(speed, -3.0f);
    CHECK(any(runtime.calls, "reckoning.begin_flip:true"));

    st.body_flipping_211 = false;
    fr.flags_2488 = 0x02000000u;
    runtime.calls.clear();
    calculate_body_flip_speed(st, fr, se, mo, reck, runtime);
    CHECK(!st.body_flipping_211);
    CHECK(runtime.calls.empty());
}

S3_TEST(post_physics_sets_apex_then_requests_upside_down_wipeout) {
    KnownAirState st = state();
    st.landing_normal_64 = {0.0f, 1.0f, 0.0f, 0.0f};
    KnownAirFrame fr = frame();
    fr.skater_up_544 = {0.0f, -1.0f, 0.0f, 0.0f};
    const KnownAirModeSettings mo = mode();
    const KnownAirWipeoutSettings wipeout_settings{0.0f, 0.0f};
    KnownAirWipeoutRequest request{false, 9.0f, 4};
    MockRuntime runtime = MockRuntime::new_();
    runtime.board_velocity = {0.0f, -2.0f, 0.0f, 0.0f};

    update_post_physics(st, fr, mo, wipeout_settings, request, runtime);

    CHECK(st.reached_apex_208);
    CHECK(request.requested_35);
    CHECK_EQ(request.scalar_116, 0.0f);
    CHECK_EQ(request.counter_200, 5u);
    CHECK_EQ(runtime.calls[0], std::string("board.velocity"));
    CHECK_EQ(runtime.calls[1], std::string("wipeout.check:true"));
}

S3_TEST(output_writes_all_samples_and_preserves_conditional_fields) {
    KnownAirState st = state();
    st.trajectory_index_216 = 2;
    st.collision_position_112 = {1.0f, 0.0f, 0.0f, 0.0f};
    st.landing_normal_64 = {1.0f, 0.0f, 0.0f, 0.0f};
    st.max_y_188 = 7.0f;
    st.start_y_184 = 2.0f;
    st.collision_time_196 = 3.0f;
    st.time_in_state_180 = 1.0f;
    const KnownAirFrame fr = frame();
    KnownAirOutput out = output();
    out.locked_trajectory_velocity_valid_452 = true;
    out.locked_trajectory_velocity_160 = {9.0f, 9.0f, 9.0f, 9.0f};
    MockRuntime runtime = MockRuntime::new_();
    runtime.prediction.trajectory.position = {};
    runtime.prediction.trajectory.velocity = {60.0f, 0.0f, 0.0f, 0.0f};
    runtime.selector_trajectory.position = {999.0f, 0.0f, 0.0f, 0.0f};
    runtime.selector_com = {};

    fill_physics_output(st, fr, out, runtime);

    CHECK_EQ(out.locked_trajectory_velocity_160, (V4{9.0f, 9.0f, 9.0f, 9.0f}));
    CHECK(out.locked_trajectory_velocity_valid_452);
    CHECK_EQ(out.jump_height_200, 5.0f);
    CHECK_EQ(out.time_until_collision_184, 2.0f);
    CHECK_EQ(out.trajectory_position_64[0], 2.0f);
    CHECK_EQ(out.trajectory_plane_samples_336[0], 0.0f);
    assert_close(out.trajectory_plane_samples_336[24], 24.0f);
    CHECK(out.known_air_valid_437);
}

}  // namespace
