// Port of skate-core/src/player/offboard/board_possession/tests.rs.
#include <algorithm>
#include <string>
#include <vector>

#include "harness.hpp"
#include "s3/core/player/offboard/board_possession.hpp"

namespace {
using namespace s3::player::offboard::board_possession;
using s3::point_graph::PointGraph;

struct Recorder : Effects {
    std::vector<std::string> calls;
    std::vector<Vector> velocities;
    void enable_animation_soft() override { calls.push_back("soft"); }
    void enable_animation_angular_only() override { calls.push_back("angular"); }
    void disable_animation() override { calls.push_back("off"); }
    void disable_linear_drive() override { calls.push_back("linear_off"); }
    void standard_board() override { calls.push_back("standard"); }
    void released_board() override { calls.push_back("released"); }
    void collision_volumes(bool e) override { calls.push_back(e ? "collision_on" : "collision_off"); }
    void clear_alignment() override { calls.push_back("unalign"); }
    void alignment(Alignment) override { calls.push_back("align"); }
    void velocity(Vector v) override {
        calls.push_back("velocity");
        velocities.push_back(v);
    }
    void position(Vector) override { calls.push_back("position"); }
    void hook_frame(Frame) override { calls.push_back("hook"); }
    void target_position_velocity(Vector) override { calls.push_back("target_velocity"); }
    void torque(Vector) override { calls.push_back("torque"); }
};

PointGraph<8> constant(float v) {
    PointGraph<8> g;
    g.x = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f};
    g.y.fill(v);
    return g;
}
Settings settings() {
    Settings s;
    s.hide_distance = 30.0f;
    s.hide_offset = 1000.0f;
    s.return_distance = 30.0f;
    s.mounted_return_distance = 5.0f;
    s.mounting_time = 0.1f;
    s.retrieval_time = constant(1.0f);
    s.retrieval_weight = constant(0.5f);
    s.throw_pitch = 18.0f;
    s.throw_velocity = constant(1.0f);
    s.throw_target_pitch = 20.0f;
    s.throw_pitch_scalar = 0.1f;
    s.throw_roll_scalar = 0.1f;
    s.throw_yaw_scalar = 0.2f;
    return s;
}
Observation observation() {
    Observation o;
    o.processed.board_frame_64 = IDENTITY;
    o.processed.player_frame_192 = IDENTITY;
    o.processed.position_592 = {};
    o.processed.velocity_912 = {};
    o.processed.direction_400 = {0.0f, 0.0f, 1.0f, 0.0f};
    o.processed.hide_direction_464 = {0.0f, 0.0f, 1.0f, 0.0f};
    o.board_collision_flags_872 = 0;
    o.board_state_840 = 0;
    o.hand_contacts = {false, false};
    o.physical_hand_positions = {};
    o.animation_board_frame_12624 = IDENTITY;
    o.animation_hand_frames = {IDENTITY, IDENTITY};
    o.attachment_frame_0 = IDENTITY;
    return o;
}
SkateboardControllerFields fields(std::uint32_t state) {
    SkateboardControllerFields f;
    f.state_448 = state;
    f.word_444 = 0;
    f.system_on_452 = true;
    return f;
}
using Calls = std::vector<std::string>;

S3_TEST(drop_runs_same_tick_first_throw_and_exactly_eight_batches) {
    State s;
    auto f = fields(1);
    auto o = observation();
    Recorder e;
    o.processed.flags_2476 = 0x3000;
    s.update(f, o, settings(), e);
    CHECK_EQ(f.state_448, 2u);
    CHECK_EQ(f.word_444, 7u);
    CHECK_EQ(Calls(e.calls.begin(), e.calls.begin() + 5),
             (Calls{"off", "released", "collision_on", "unalign", "velocity"}));
    for (int i = 0; i < 8; ++i) s.update(f, o, settings(), e);
    CHECK_EQ(std::count(e.calls.begin(), e.calls.end(), std::string("torque")), 24);
    CHECK_EQ(f.word_444, 0u);
}

S3_TEST(recall_wins_over_distance_hide_and_mount_clamps_duration_down) {
    State s;
    auto f = fields(2);
    auto o = observation();
    Recorder e;
    o.processed.board_frame_64[3] = {100.0f, 0.0f, 0.0f, 7.0f};
    o.processed.flags_2480 = 0x80000;
    s.update_state(f, o, settings(), e);
    CHECK_EQ(f.state_448, 4u);
    CHECK_EQ(s.retrieval.duration_196, 0.1f);
    CHECK(s3::rs::abs(s.retrieval.initial_0[3][0] - 5.0f) < 1e-5f);
    CHECK_NE(s.retrieval.initial_0[3][3], 0.0f);
    CHECK_EQ(e.calls, (Calls{"collision_off", "unalign", "angular", "position"}));
}

S3_TEST(returning_contact_precedes_completion_but_mount_ignores_contact) {
    State s;
    auto f = fields(4);
    auto o = observation();
    Recorder e;
    s.retrieval.progress_200 = 1.0f;
    o.hand_contacts[1] = true;
    s.update_state(f, o, settings(), e);
    CHECK_EQ(f.state_448, 2u);
    f.state_448 = 4;
    o.processed.flags_2480 = 0x80000;
    e.calls.clear();
    s.update_state(f, o, settings(), e);
    CHECK_EQ(f.state_448, 1u);
    CHECK_EQ(e.calls, (Calls{"velocity", "soft", "standard", "collision_on"}));
}

S3_TEST(unordered_progress_completes_and_only_selected_hand_is_armed) {
    State s;
    auto f = fields(4);
    const auto o = observation();
    Recorder e;
    s.retrieval.progress_200 = s3::rs::f32::NAN_;
    s.update_state(f, o, settings(), e);
    CHECK_EQ(s.selected_hand_424, 1u);
    using D = std::array<std::array<std::uint32_t, 4>, 2>;
    CHECK_EQ(s.hands[0].dynamics, (D{{{0, 0, 0, 2}, {0, 0, 0, 2}}}));
    CHECK_EQ(s.hands[1].dynamics, (D{{{0x4395ffff, 0, 0x468c9fff, 2}, {0x4395ffff, 0, 0x468c9fff, 2}}}));
}

S3_TEST(stop_preserves_throw_word_from_release_and_off_controller_is_inert) {
    State s;
    auto f = fields(1);
    auto o = observation();
    Recorder e;
    o.processed.flags_2476 = 0x1000;
    s.stop(f, o, settings(), e);
    CHECK_EQ(f.word_444, 8u);
    CHECK_EQ(f.state_448, 0u);
    CHECK(!f.system_on_452);
    e.calls.clear();
    s.update(f, o, settings(), e);
    s.stop(f, o, settings(), e);
    CHECK(e.calls.empty());
    CHECK_EQ(f.word_444, 8u);
}

S3_TEST(angular_request_rejects_orthogonal_omega_and_retains_singular_behavior) {
    const std::array<Vector, 3> tensor{{{2.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 3.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 5.0f, 0.0f}}};
    const auto a = angular::acceleration_delta({0.1f, 0.0f, 0.0f, 0.0f}, {2.0f, 99.0f, 0.0f, 0.0f}, tensor);
    CHECK(s3::rs::abs(a[0] - 240.0f) < 0.01f);
    CHECK_EQ(a[1], 0.0f);
    const auto singular = angular::acceleration_delta({0.1f, 0.0f, 0.0f, 0.0f}, {}, {});
    CHECK(std::isnan(singular[0]) || std::isnan(singular[1]) || std::isnan(singular[2]));
}

S3_TEST(fill_uses_hidden_initial_position_and_zero_axis_quadrant) {
    State s;
    const auto o = observation();
    s.retrieval.initial_0[3] = {1.0f, 0.0f, 0.0f, 0.0f};
    const auto result = fill(fields(3), s, o.processed, IDENTITY);
    CHECK_EQ(s3::rs::to_bits(result.angle_36), 0x3fc90fdbu);
    CHECK(result.free_312 && result.hiding_321);
    CHECK_EQ(s3::rs::to_bits(math::quadrant_angle(-0.0f, 0.0f)), 0xbfc90fdbu);
}
}  // namespace
