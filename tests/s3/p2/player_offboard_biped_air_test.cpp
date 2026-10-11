// Port of skate-core/src/player/offboard/biped_air/collision_tests.rs and
// biped_air/post/tests.rs.
#include <algorithm>

#include "harness.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/player/offboard/biped_air.hpp"

namespace {
using namespace s3::player::offboard::biped_air::recovered;
using s3::player::offboard::air_launch::Packet;
using s3::player::wipeout::Frame;
using s3::player::wipeout::Mode;
using s3::player::wipeout::Requests;
using namespace s3::player::offboard::biped_air::recovered::post::checks;

// ---- collision_tests.rs
S3_TEST(vertical_error_requests_bail_without_planar_restart) {
    State state;
    state.result.velocity_288 = {0.0f, -3.0f, 0.0f, 0.0f};
    const auto response = state.collision_response({{{0.0f, 0.21f, 0.0f, 0.0f}, {}}}, {0.0f, 1.0f, 0.0f, 0.0f}, true);
    CHECK(response.request_52);
    CHECK(!response.restart.has_value());
}

S3_TEST(restart_changes_launch_and_current_sample_without_rewriting_secondary_velocity) {
    State state;
    state.result.velocity_288 = {-3.0f, 1.0f, 0.0f, 0.0f};
    state.result.position_272 = {2.0f, 3.0f, 4.0f, 0.0f};
    const auto response = state.collision_response({{{0.02f, 0.0f, 0.0f, 0.0f}, {}}}, {0.0f, 1.0f, 0.0f, 0.0f}, true);
    const auto normal = s3::rs::unwrap(response.restart);
    auto packet = Packet::initialized(0.0f);
    packet.secondary_velocity_16 = {5.0f, 6.0f, 7.0f, 0.0f};
    const auto result = state.restart_packet(packet, normal, {2.0f, 3.0f, 4.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, -0.8f);
    CHECK_EQ(result.secondary_velocity_16, (Vector{5.0f, 6.0f, 7.0f, 0.0f}));
    const float expected[3] = {1.0f, 1.0f, 0.0f};
    for (int k = 0; k < 3; ++k) CHECK(s3::rs::abs(result.velocity_0[k] - expected[k]) < 1e-5f);
    CHECK(result.has_board_position_116);
    const auto retained = state.result.velocity_288;
    state.correct_restarted_sample();
    CHECK_EQ(state.result.velocity_288, retained);
    CHECK(s3::rs::abs(state.result.position_272[0] - (2.0f + 3.0f * DT)) < 1e-6f);
}

// ---- post/tests.rs
Frame frame() {
    Frame f;
    f.category = 500;
    f.timestep = DT;
    f.animation_up = {0.0f, 1.0f, 0.0f, 0.0f};
    f.deck = s3::physics::skeleton_animation_record::IDENTITY;
    f.input_board = s3::physics::skeleton_animation_record::IDENTITY;
    f.world_to_animation = s3::physics::skeleton_animation_record::IDENTITY;
    f.system_up_y = 1.0f;
    return f;
}
Mode mode() { return Mode{true, false, 0.0f, 0.0f}; }
Settings settings() {
    return Settings{Thresholds{2.0f, 3.0f, 4.0f, 5.0f}, Thresholds{6.0f, 7.0f, 8.0f, 9.0f}, 10.0f};
}

S3_TEST(offboard_speed_is_strict_root_velocity_and_mode_always_changes) {
    auto f = frame();
    f.maximum_pose_error = 100.0f;
    f.com_velocity = {100.0f, 100.0f, 100.0f, 100.0f};
    auto r = Requests::new_();
    offboard_air(r, settings(), mode(), f, {10.0f, 0.0f, 0.0f, 900.0f});
    CHECK_EQ(r.mode, 4u);
    CHECK_EQ(r.count, 0u);
    offboard_air(r, settings(), mode(), f, {10.01f, 0.0f, 0.0f, 0.0f});
    CHECK_EQ(r.count, 1u);
    CHECK(r.reasons[18]);
}

S3_TEST(squash_displacement_and_force_are_exclusive) {
    auto f = frame();
    f.maximum_pose_error = 10.0f;
    f.pose_error = {10.0f, 0.0f, 0.0f, 0.0f};
    f.regions_force.fill(20.0f);
    auto r = Requests::new_();
    skeleton_air(r, settings(), mode(), f);
    CHECK_EQ(r.count, 1u);
    CHECK(r.reasons[18] && !r.reasons[1] && !r.reasons[0]);
    auto m = mode();
    m.check_squash = false;
    skeleton_air(r, settings(), m, f);
    CHECK_EQ(r.count, 2u);
    CHECK(r.reasons[1] && !r.reasons[0]);
    f.pose_error = {3.0f, 0.0f, 0.0f, 100.0f};
    skeleton_air(r, settings(), m, f);
    CHECK_EQ(r.count, 3u);
    CHECK(r.reasons[0]);
    CHECK_EQ(r.mode, 0u);
}

S3_TEST(offboard_uses_its_own_body_limit_and_stock_ob_arm_limit) {
    auto f = frame();
    auto r = Requests::new_();
    f.regions_force[2] = 9.0f;
    offboard_air(r, settings(), mode(), f, {11.0f, 0.0f, 0.0f, 0.0f});
    CHECK_EQ(r.count, 0u);
    f.regions_force[2] = 9.01f;
    offboard_air(r, settings(), mode(), f, {11.0f, 0.0f, 0.0f, 0.0f});
    CHECK_EQ(r.count, 1u);
    CHECK(r.reasons[0]);
}

S3_TEST(unordered_displacement_retains_native_else_branch) {
    auto f = frame();
    f.pose_error[0] = s3::rs::f32::NAN_;
    auto r = Requests::new_();
    skeleton_air(r, settings(), mode(), f);
    CHECK(r.reasons[1]);
    CHECK_EQ(r.count, 1u);
}

S3_TEST(air_post_does_not_deduplicate_repeated_request_31) {
    State s;
    s.result.valid_404 = true;
    s.result.velocity_288[1] = -1.0f;
    s.result.contact_velocity_320 = {0.0f, 0.0f, 21.0f, 0.0f};
    s.flags_544_550[5] = true;
    s.time_remaining_444 = -1.0f;
    auto r = Requests::new_();
    post::PostInput in;
    in.flags_2484 = 0;
    in.state_timer_2664 = 0.0f;
    in.forward_224 = {0.0f, 0.0f, 1.0f, 0.0f};
    in.side_192 = {1.0f, 0.0f, 0.0f, 0.0f};
    s.post_physics(in, settings(), frame(), mode(), {}, r);
    CHECK_EQ(r.count, 3u);
    CHECK(r.reasons[31]);
    CHECK_EQ(std::count(r.reasons.begin(), r.reasons.end(), true), 1);
    CHECK_EQ(r.mode, 4u);
}
}  // namespace
