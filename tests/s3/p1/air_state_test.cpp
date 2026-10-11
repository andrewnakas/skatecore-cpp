// Port of skate-core/src/air/state/tests/mod.rs.
#include <array>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "harness.hpp"
#include "s3/core/air/state.hpp"
#include "s3/core/point_graph.hpp"

namespace {

using namespace s3::air::state;
using s3::point_graph::PointGraph;
namespace rs = s3::rs;
using V4 = std::array<float, 4>;

std::string b(bool v) { return v ? "true" : "false"; }
std::string f(float v) {
    std::ostringstream os;
    os << v;
    return os.str();
}

struct FakeLaunchInfo final : PhysicsAirLaunchInfo {
    V4 start_velocity{};
    V4 animation_position{};
    V4 trajectory_override{};
    V4 board_override{};
    std::array<float, 2> cone_angles_{};
    std::vector<bool> jumped_writes;
    bool use_override = false;
    std::uint16_t trajectory_count = 0;

    void set_start_velocity(V4 velocity) override { start_velocity = velocity; }
    V4 centre_of_mass_animation_position() const override { return animation_position; }
    void set_trajectory_start_position_override(V4 position) override { trajectory_override = position; }
    void set_board_position_override(V4 position) override { board_override = position; }
    std::array<float, 2> cone_angles() const override { return cone_angles_; }
    void set_cone_angles(std::array<float, 2> angles) override { cone_angles_ = angles; }
    void set_player_jumped(bool jumped) override { jumped_writes.push_back(jumped); }
    void set_use_trajectory_start_position_override(bool enabled) override { use_override = enabled; }
    void set_trajectory_count(std::uint16_t count) override { trajectory_count = count; }
};

PhysicsAirReckoningFields reckoning() {
    PhysicsAirReckoningFields r;
    r.current_landing_normal_1152 = {0.0f, 1.0f, 0.0f, 0.0f};
    r.collision_reference_normal_1216 = {0.0f, 0.0f, 1.0f, 0.0f};
    r.body_spin_angle_1568 = 2.0f;
    r.body_spin_speed_1572 = 3.0f;
    return r;
}

struct FakeRuntime final : PhysicsAirRuntime<FakeLaunchInfo> {
    std::vector<std::string> events;
    float board_height = 0.0f;
    V4 board_velocity{};
    bool query_started = false;
    std::optional<V4> selector_normal;
    bool selector_com_ready = false;
    float angle = 0.0f;
    bool collision = false;
    AirBoardForce collision_force = AirBoardForce::ZERO;
    std::optional<std::pair<std::uint32_t, AirBoardForce>> enqueued_force;
    std::optional<V4> set_velocity;
    std::optional<FakeLaunchInfo> launched;
    std::optional<std::tuple<V4, float, float, float>> reckoning_call;
    PhysicsAirReckoningFields reckoning_output = reckoning();
    std::optional<V4> collision_reference;
    std::optional<V4> known_air_position;
    std::optional<float> forced_squared_length;
    std::optional<float> forced_length;
    std::optional<V4> forced_clamp;

    // PhysicsAirMath
    float minimum_vminfp(float left, float right) override { return left < right ? left : right; }
    float length_squared_vmsum3fp(V4 value) override {
        return forced_squared_length.value_or(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
    }
    float length_vmsum3fp_vrsqrte(V4 value) override {
        return forced_length.value_or(rs::sqrt(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]));
    }
    V4 clamp_vector_within_max_length(V4 value, float maximum_length) override {
        if (forced_clamp) {
            return *forced_clamp;
        }
        const float length = rs::sqrt(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
        if (length > maximum_length) {
            V4 out{};
            for (std::size_t i = 0; i < 4; ++i) out[i] = value[i] * (maximum_length / length);
            return out;
        }
        return value;
    }

    // PhysicsAirRuntime
    void set_air_collision_update_enabled(bool enabled) override { events.push_back("collision-enabled:" + b(enabled)); }
    void set_skeleton_collision_state(std::uint32_t state) override {
        events.push_back("collision-state:" + std::to_string(state));
    }
    void set_skeleton_inverse_kinematics_enabled(bool enabled) override { events.push_back("ik:" + b(enabled)); }
    void enable_board_angular_drive_only() override { events.push_back("angular-drive"); }
    void set_footplant_flag_240(bool value) override { events.push_back("footplant-240:" + b(value)); }
    void reset_footplants() override { events.push_back("footplant-reset"); }
    float board_transform_height() override {
        events.push_back("board-height");
        return board_height;
    }
    void request_skeleton_heading_update() override { events.push_back("heading"); }
    void update_air_collision() override { events.push_back("air-collision"); }
    bool trajectory_query_just_started() const override { return query_started; }
    FakeLaunchInfo construct_trajectory_launch_info() override {
        events.push_back("launch-construct");
        return FakeLaunchInfo{};
    }
    void fill_skeleton_launch_info(FakeLaunchInfo& info) override {
        events.push_back("launch-fill");
        info.animation_position = {10.0f, 20.0f, 30.0f, 40.0f};
        info.cone_angles_ = {2.0f, 4.0f};
    }
    void launch_trajectory(const FakeLaunchInfo& info) override {
        events.push_back("launch");
        launched = info;
    }
    void update_trajectory_selector() override { events.push_back("selector-update"); }
    std::optional<V4> selector_landing_normal() const override { return selector_normal; }
    bool selector_centre_of_mass_trajectory_ready() const override { return selector_com_ready; }
    float angle_between_vectors(V4, V4) override {
        events.push_back("angle");
        return angle;
    }
    PhysicsAirReckoningFields update_reckoning_air_states(V4 landing_normal, float normal_blend, float body_spin,
                                                          float body_flip) override {
        events.push_back("reckoning");
        reckoning_call = std::make_tuple(landing_normal, normal_blend, body_spin, body_flip);
        return reckoning_output;
    }
    void update_known_air_skeleton(V4 trajectory_position) override {
        events.push_back("known-air");
        known_air_position = trajectory_position;
    }
    void update_animated_skateboard_skeleton(bool argument) override {
        events.push_back("animated-board:" + b(argument));
    }
    void update_board_steering_tilt(float tilt) override { events.push_back("steering-tilt:" + f(tilt)); }
    bool calculate_air_collision_force(V4 reference_normal, AirBoardForce& force) override {
        events.push_back("collision-force");
        collision_reference = reference_normal;
        force = collision_force;
        return collision;
    }
    void enqueue_board_force(std::uint32_t tag, AirBoardForce force) override {
        events.push_back("enqueue-force");
        enqueued_force = std::make_pair(tag, force);
    }
    void set_board_velocity(V4 velocity) override {
        events.push_back("set-board-velocity");
        set_velocity = velocity;
    }
    void enable_skateboard_error_on_skeleton() override { events.push_back("board-error"); }
    V4 board_body_velocity() override {
        events.push_back("board-velocity");
        return board_velocity;
    }
    void check_for_air_wipeout(bool argument) override { events.push_back("wipeout:" + b(argument)); }
};

PhysicsAirFrame frame() {
    PhysicsAirFrame fr;
    fr.current_velocity_400 = {1.0f, 2.0f, 3.0f, 0.0f};
    fr.ground_position_y_500 = 4.0f;
    fr.trajectory_position_592 = {1.0f, 5.0f, 2.0f, 0.0f};
    fr.trajectory_velocity_608 = {2.0f, 10.0f, 4.0f, 0.0f};
    fr.jump_velocity_848 = {2.0f, 8.0f, 4.0f, 0.0f};
    fr.flags_2468 = 0;
    fr.previous_physics_state_2504 = 500;
    fr.previous_physics_category_2516 = 0;
    fr.frames_since_jump_correction_2576 = 12;
    fr.delta_time_2604 = 0.1f;
    fr.body_spin_input_2640 = 3.0f;
    fr.gravity_y_2648 = -10.0f;
    fr.state_timer_2664 = 1.0f;
    return fr;
}

PhysicsAirState state() {
    PhysicsAirState s;
    s.centre_of_mass_trajectory = AirTrajectory{{90.0f, 90.0f, 90.0f, 90.0f}, {80.0f, 80.0f, 80.0f, 80.0f},
                                                {70.0f, 70.0f, 70.0f, 70.0f}, 60.0f};
    s.landing_normal = {9.0f, 9.0f, 9.0f, 9.0f};
    s.time_in_state = 9.0f;
    s.start_y = 9.0f;
    s.max_y = 9.0f;
    s.reached_apex = true;
    s.use_centre_of_mass_velocity = false;
    s.selector_latch_174 = true;
    s.trajectory_query_countdown = 9;
    return s;
}

PhysicsAirSettings settings() {
    PhysicsAirSettings s;
    s.body_spin_over_time_320.x = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f};
    s.body_spin_over_time_320.y.fill(2.0f);
    s.landing_normal_blend_388 = 0.25f;
    s.body_spin_scale_428 = 4.0f;
    s.landing_normal_angle_limit_444 = 1.0f;
    return s;
}

using Events = std::vector<std::string>;

S3_TEST(enter_preserves_order_and_offboard_velocity_rule) {
    PhysicsAirState st = state();
    const PhysicsAirFrame fr = frame();
    FakeRuntime runtime;
    runtime.board_height = 6.0f;
    enter(st, fr, {0.0f, -9.0f, 0.0f, 0.0f}, runtime);

    CHECK_EQ(runtime.events, (Events{
                                 "collision-enabled:true",
                                 "collision-state:7",
                                 "ik:true",
                                 "collision-state:6",
                                 "angular-drive",
                                 "footplant-240:false",
                                 "footplant-reset",
                                 "board-height",
                                 "heading",
                             }));
    CHECK(st.use_centre_of_mass_velocity);
    CHECK_EQ(st.centre_of_mass_trajectory.velocity, (V4{2.0f, 3.0f, 4.0f, 0.0f}));
    CHECK_EQ(st.landing_normal, (V4{0.0f, 1.0f, 0.0f, 0.0f}));
    CHECK_EQ(st.start_y, 4.0f);
    CHECK_EQ(st.max_y, 6.0f);
    CHECK(!st.reached_apex && !st.selector_latch_174);
    CHECK_EQ(st.trajectory_query_countdown, 0);
}

S3_TEST(category_400_can_select_com_without_requesting_heading) {
    PhysicsAirState st = state();
    PhysicsAirFrame fr = frame();
    fr.previous_physics_category_2516 = 400;
    fr.previous_physics_state_2504 = 100;
    FakeRuntime runtime;

    enter(st, fr, V4{}, runtime);
    CHECK(st.use_centre_of_mass_velocity);
    bool any_heading = false;
    for (const auto& e : runtime.events) any_heading = any_heading || e == "heading";
    CHECK(!any_heading);
}

S3_TEST(jump_velocity_uses_current_vertical_and_clamped_current_horizontal) {
    PhysicsAirFrame fr = frame();
    fr.current_velocity_400 = {10.0f, 4.0f, 0.0f, 7.0f};
    fr.jump_velocity_848 = {2.0f, 10.0f, 0.0f, 5.0f};
    FakeRuntime runtime;
    runtime.forced_squared_length = 64.0f;
    runtime.forced_length = 2.0f;
    runtime.forced_clamp = V4{2.0f, 0.0f, 0.0f, 99.0f};

    const V4 result = calculate_velocity_from_jump(fr, 2, runtime);
    CHECK_EQ(result, (V4{2.0f, 4.0f, 0.0f, 5.0f}));
}

S3_TEST(update_runs_com_launch_and_fixed_step_in_native_order) {
    PhysicsAirState st = state();
    st.use_centre_of_mass_velocity = true;
    st.selector_latch_174 = false;
    st.trajectory_query_countdown = 0;
    st.time_in_state = 0.25f;
    st.max_y = 5.0f;
    st.centre_of_mass_trajectory =
        AirTrajectory{{1.0f, 2.0f, 3.0f, 0.0f}, {4.0f, 5.0f, 6.0f, 0.0f}, {0.0f, -6.0f, 0.0f, 0.0f}, -1.0f};
    PhysicsAirFrame fr = frame();
    fr.frames_since_jump_correction_2576 = 12;
    FakeRuntime runtime;
    runtime.board_height = 7.0f;
    runtime.selector_normal = V4{0.0f, 0.8f, 0.2f, 0.0f};
    runtime.reckoning_output = reckoning();
    runtime.reckoning_output.collision_reference_normal_1216 = {1.0f, 0.0f, 0.0f, 0.0f};

    PhysicsAirReckoningFields reck = reckoning();
    update(st, fr, settings(), reck, {0.0f, -6.0f, 0.0f, 0.0f}, runtime);

    CHECK_EQ(runtime.events, (Events{
                                 "air-collision",
                                 "launch-construct",
                                 "launch-fill",
                                 "launch-fill",
                                 "launch",
                                 "selector-update",
                                 "angle",
                                 "reckoning",
                                 "known-air",
                                 "steering-tilt:0",
                                 "collision-force",
                                 "board-error",
                                 "board-height",
                             }));
    const FakeLaunchInfo launch = rs::unwrap(runtime.launched);
    CHECK_EQ(launch.jumped_writes, (std::vector<bool>{true, false}));
    CHECK_EQ(launch.trajectory_override, (V4{10.0f, 19.35f, 30.0f, 40.0f}));
    CHECK_EQ(launch.board_override, (V4{10.0f, 19.2f, 30.0f, 40.0f}));
    CHECK_EQ(launch.cone_angles_, (std::array<float, 2>{0.8f, 1.6f}));
    CHECK(launch.use_override);
    CHECK_EQ(launch.trajectory_count, 5);
    CHECK_EQ(st.trajectory_query_countdown, 7);
    CHECK_EQ(st.max_y, 7.0f);
    CHECK_EQ(st.time_in_state, 0.35f);
    // Board collision used the pre-update reckoning normal
    CHECK(runtime.collision_reference == std::optional<V4>(V4{1.0f, 0.0f, 0.0f, 0.0f}));
}

S3_TEST(skateboard_collision_force_wins_over_direct_velocity_correction) {
    const PhysicsAirState st = state();
    AirBoardForce force;
    force.force_world = {1.0f, 2.0f, 3.0f, 4.0f};
    force.point_board_local = {5.0f, 6.0f, 7.0f, 8.0f};
    FakeRuntime runtime;
    runtime.collision = true;
    runtime.collision_force = force;
    PhysicsAirFrame fr = frame();
    fr.frames_since_jump_correction_2576 = 0;

    update_skateboard(st, fr, reckoning(), runtime);
    CHECK(runtime.enqueued_force.has_value());
    CHECK_EQ(runtime.enqueued_force->first, 15u);
    CHECK(runtime.enqueued_force->second == force);
    CHECK(!runtime.set_velocity.has_value());
}

S3_TEST(skateboard_miss_applies_jump_velocity_only_before_frame_twelve) {
    PhysicsAirState st = state();
    st.use_centre_of_mass_velocity = false;
    PhysicsAirFrame fr = frame();
    fr.frames_since_jump_correction_2576 = 2;
    fr.current_velocity_400 = {2.0f, 4.0f, 4.0f, 0.0f};
    fr.jump_velocity_848 = {2.0f, 10.0f, 4.0f, 0.0f};
    FakeRuntime runtime;
    runtime.forced_squared_length = 0.0f;

    update_skateboard(st, fr, reckoning(), runtime);
    CHECK(runtime.set_velocity == std::optional<V4>(V4{2.0f, 4.0f, 4.0f, 0.0f}));
    CHECK_EQ(runtime.events, (Events{"steering-tilt:0", "collision-force", "set-board-velocity"}));
}

S3_TEST(post_physics_latches_only_strict_downward_velocity_then_checks_wipeout) {
    PhysicsAirState st = state();
    st.reached_apex = false;
    FakeRuntime runtime;
    runtime.board_velocity = {0.0f, -0.0f, 0.0f, 0.0f};
    update_post_physics(st, runtime);
    CHECK(!st.reached_apex);
    runtime.board_velocity[1] = -0.01f;
    update_post_physics(st, runtime);
    CHECK(st.reached_apex);
    CHECK_EQ(runtime.events, (Events{"board-velocity", "wipeout:false", "board-velocity", "wipeout:false"}));
}

S3_TEST(exit_and_output_publish_only_recovered_fields) {
    PhysicsAirState st = state();
    st.max_y = 9.0f;
    st.start_y = 4.0f;
    st.reached_apex = true;
    st.selector_latch_174 = true;
    st.landing_normal = {1.0f, 2.0f, 3.0f, 4.0f};
    PhysicsAirReckoningFields reck = reckoning();

    const PhysicsAirOutput output = fill_physics_output(st);
    CHECK_EQ(output.jump_height, 5.0f);
    CHECK(output.scalar_184_write.has_value());
    CHECK_EQ(rs::to_bits(*output.scalar_184_write), 0x40800000u);
    CHECK_EQ(output.landing_normal, (V4{1.0f, 2.0f, 3.0f, 4.0f}));

    exit(st, reck);
    CHECK(!st.use_centre_of_mass_velocity);
    CHECK_EQ(reck.body_spin_angle_1568, 0.0f);
    CHECK_EQ(reck.body_spin_speed_1572, 0.0f);
}

S3_TEST(angle_wrap_uses_recovered_turn_constants) {
    CHECK_EQ(rs::to_bits(wrap_signed_angle(0.0f)), rs::to_bits(0.0f));
    CHECK(wrap_signed_angle(4.0f) < 0.0f);
    CHECK(wrap_signed_angle(2.0f) > 0.0f);
}

}  // namespace
