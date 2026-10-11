// Port of skate-core/src/player/wipeout/tests.rs.
#include <algorithm>

#include "harness.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/player/wipeout.hpp"

namespace {
using namespace s3::player::wipeout;
using s3::physics::skeleton_animation_record::IDENTITY;
using s3::point_graph::PointGraph;

Frame input() {
    Frame f{};
    f.flags_2468 = 0;
    f.flags_2472 = 0;
    f.flags_2476 = 0;
    f.flags_2480 = 0;
    f.flags_2484 = 0;
    f.category = 100;
    f.timestep = 1.0f / 60.0f;
    f.time_on_ground = 1.0f;
    f.speed = 0.0f;
    f.animation_up = {0.0f, 1.0f, 0.0f, 0.0f};
    f.landing_angle = 0.0f;
    f.deck_velocity = {};
    f.com_velocity = {};
    f.jump_fix_frames = 1000;
    f.deck = IDENTITY;
    f.input_board = IDENTITY;
    f.world_to_animation = IDENTITY;
    f.closing_velocity = {};
    f.board_material_flags = 0;
    f.board_contact = true;
    f.wheel_contact = true;
    f.board_contact_normal = {0.0f, 1.0f, 0.0f, 0.0f};
    f.opposing_contact = 0.0f;
    f.regions_force = {};
    f.maximum_skater_force = 0.0f;
    f.vehicle_force = 0.0f;
    f.group_8 = false;
    f.conflicting = false;
    f.compliant = false;
    f.highest_normal = {0.0f, 1.0f, 0.0f, 0.0f};
    f.pose_error = {};
    f.maximum_pose_error = 0.0f;
    f.flip_active = false;
    f.flip_requested_speed = 0.0f;
    f.system_up_y = 1.0f;
    f.grind_selected = false;
    f.grind_normal_valid = false;
    f.grind_normal = {0.0f, 1.0f, 0.0f, 0.0f};
    return f;
}
Mode mode() { return Mode{true, true, 10.0f, 1.0f}; }
Settings settings() {
    Settings s{};
    s.ground = GroundSettings{1.0f, 100.0f, 100.0f, 1.0f, 1.0f, 1.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f,
                              1.0f, 1.0f, 10.0f, 1.0f, 1.0f, 1.0f, 1.0f, 100.0f, 1.0f, 1.0f};
    s.air.xz_trick = 10.0f;
    s.air.y_trick = 10.0f;
    s.air.xz_acceleration = 10.0f;
    s.air.y_acceleration = 10.0f;
    s.air.max_squash = 100.0f;
    s.air.max_displacement = 100.0f;
    s.air.max_contact = 100.0f;
    s.air.max_arm_contact = 100.0f;
    s.air.body_flip_scalar = 1.0f;
    s.air.body_flip_acc_scalar = 1.0f;
    s.air.light_dmo_scalar = 1.0f;
    s.air.ignore_danger_frames = 10;
    s.air.max_landing_speed = 6.0f;
    s.air.max_stairs_speed = 8.0f;
    s.air.max_grind_speed = 5.0f;
    s.air.max_landing_angle = PointGraph<8>{{0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f},
                                            {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}};
    s.lean_contact_y = -1.0f;
    return s;
}
RequestInput request_input() { return RequestInput{0, 0, 0, 0, 1.0f, 100}; }
std::size_t count_true(const std::array<bool, 34>& r) { return std::size_t(std::count(r.begin(), r.end(), true)); }

S3_TEST(handplant_air_check_preserves_contact_threshold_and_real_impact_bails) {
    Frame f = input();
    f.category = 600;
    f.closing_velocity = {0.2f, 0.0f, 0.0f, 0.0f};
    Settings s = settings();
    s.air.xz_trick = 0.1f;
    s.air.y_trick = 0.6f;
    Requests foot = Requests::new_();
    check_plant(foot, s, f);
    CHECK(foot.reasons[2]);
    Requests hand = Requests::new_();
    check_air(hand, s, mode(), f, false);
    CHECK_EQ(hand.count, 0u);
    f.closing_velocity = {11.0f, 0.0f, 0.0f, 0.0f};
    check_air(hand, s, mode(), f, false);
    CHECK(hand.reasons[2]);
}

S3_TEST(wipeout_ground_priority_and_repeated_force_change_runout_selection) {
    Requests state = Requests::new_();
    Frame f = input();
    const Settings s = settings();
    f.closing_velocity = {11.0f, 0.0f, 0.0f, 0.0f};
    check_ground(state, s, mode(), f);
    CHECK_EQ(state.count, 1u);
    CHECK(state.reasons[2]);
    CHECK(state.requests_runout(request_input()));
    CHECK(!state.requests_wipeout(request_input()));
    state.clear_after_selection();
    f.maximum_pose_error = 101.0f;
    f.pose_error = {101.0f, 0.0f, 0.0f, 0.0f};
    f.regions_force[0] = 101.0f;
    check_ground(state, s, mode(), f);
    CHECK_EQ(state.count, 3u);
    CHECK(state.reasons[18] && state.reasons[2] && state.reasons[0]);
    CHECK(!state.reasons[1]);
    CHECK(!state.requests_runout(request_input()));
    CHECK(state.requests_wipeout(request_input()));
    state.clear_after_selection();
    f.maximum_pose_error = 0.0f;
    f.pose_error = {};
    check_ground(state, s, mode(), f);
    CHECK_EQ(state.count, 3u);
    CHECK_EQ(count_true(state.reasons), std::size_t(2));
}

S3_TEST(wipeout_air_landing_uses_selected_velocity_and_preserves_impact_value) {
    Requests state = Requests::new_();
    Frame f = input();
    const Settings s = settings();
    f.deck_velocity = {0.0f, -2.0f, 0.0f, 0.0f};
    f.com_velocity = {0.0f, -7.0f, 0.0f, 0.0f};
    check_air(state, s, mode(), f, false);
    CHECK_EQ(state.count, 0u);
    check_air(state, s, mode(), f, true);
    CHECK_EQ(state.count, 1u);
    CHECK(state.reasons[6]);
    CHECK_EQ(state.values[6], 7.0f);
    state.clear_after_selection();
    f.grind_selected = true;
    f.grind_normal_valid = true;
    f.grind_normal = {1.0f, 0.0f, 0.0f, 0.0f};
    check_air(state, s, mode(), f, true);
    CHECK_EQ(state.count, 0u);
    f.grind_normal_valid = false;
    check_air(state, s, mode(), f, true);
    CHECK_EQ(state.count, 1u);
    CHECK(state.reasons[8]);
    CHECK(!state.reasons[6]);
}

S3_TEST(wipeout_sensitive_conditions_count_both_triggers_and_clear_only_requests) {
    Requests state = Requests::new_();
    Frame f = input();
    f.flags_2476 = 1u << 26;
    f.flags_2472 = 1u << 6;
    f.compliant = true;
    check_air(state, settings(), mode(), f, false);
    CHECK_EQ(state.count, 2u);
    CHECK(state.reasons[21]);
    CHECK_EQ(count_true(state.reasons), std::size_t(1));
    state.balance = 3.0f;
    state.cooldown = 0.2f;
    state.clear_after_selection();
    CHECK_EQ(state.count, 0u);
    CHECK_EQ(state.balance, 3.0f);
    CHECK_EQ(state.cooldown, 0.2f);
    CHECK_EQ(state.mode, 2u);
    CHECK_EQ(state.contact_frames, 3);
    state.reset_systems();
    CHECK_EQ(state.mode, 0u);
    CHECK_EQ(state.balance, 3.0f);
    CHECK_EQ(state.cooldown, 0.2f);
}

S3_TEST(wipeout_cooldown_has_native_initial_and_teleport_lifetimes) {
    Requests state = Requests::new_();
    state.initialize_player();
    CHECK_EQ(s3::rs::to_bits(state.cooldown), 0x3d23d70au);
    Frame f = input();
    f.maximum_pose_error = 101.0f;
    check_air(state, settings(), mode(), f, false);
    CHECK_EQ(state.count, 0u);
    CHECK_EQ(state.contact_frames, 3);
    CHECK(state.cooldown > 0.0f);
    state.teleport();
    CHECK_EQ(s3::rs::to_bits(state.cooldown), 0x3ecccccdu);
    state.reset_systems();
    CHECK_EQ(s3::rs::to_bits(state.cooldown), 0x3ecccccdu);
    state.cooldown = 0.001f;
    check_ground(state, settings(), mode(), f);
    CHECK(state.cooldown < 0.0f);
    CHECK_EQ(state.count, 0u);
    check_ground(state, settings(), mode(), f);
    CHECK_EQ(state.count, 1u);
    CHECK(state.reasons[18]);
}

S3_TEST(wipeout_ground_animation_keeps_additional_checks_during_cooldown) {
    Requests state = Requests::new_();
    state.teleport();
    Frame f = input();
    f.regions_force[0] = 101.0f;
    f.opposing_contact = 1.5f;
    check_ground_animation(state, settings(), mode(), f, 1.0f);
    CHECK_EQ(state.count, 2u);
    CHECK(state.reasons[0] && state.reasons[20]);
    state.clear_after_selection();
    check_ground_animation(state, settings(), mode(), f, 2.0f);
    CHECK_EQ(state.count, 1u);
    CHECK(state.reasons[0]);
    CHECK(!state.reasons[20]);
}
}  // namespace
