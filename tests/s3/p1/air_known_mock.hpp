// Port of skate-core/src/air/known/tests/mock.rs (test-only helper).
#pragma once
#include <array>
#include <string>
#include <utility>
#include <vector>

#include "s3/core/air/known.hpp"
#include "s3/core/point_graph.hpp"

namespace air_known_mock {

using namespace s3::air::known;
using s3::point_graph::PointGraph;

inline PointGraph<8> flat_graph(float value) {
    PointGraph<8> g;
    g.x = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f};
    g.y.fill(value);
    return g;
}

inline KnownAirTrajectory trajectory() {
    KnownAirTrajectory t;
    t.position = {};
    t.velocity = {};
    t.acceleration = {};
    t.scalar_48 = 1.0f;
    t.word_52 = 0;
    t.word_56 = 0;
    t.word_60 = 0;
    return t;
}

inline KnownAirState state() {
    KnownAirState s;
    s.landing_normal_64 = {0.0f, 1.0f, 0.0f, 0.0f};
    s.landing_heading_80 = {1.0f, 0.0f, 0.0f, 0.0f};
    s.trajectory_apex_96 = {};
    s.collision_position_112 = {};
    s.selector_vector_128 = {};
    s.trajectory_follow_offset_144 = {};
    s.target_com_position_160 = {};
    s.collision_normal_speed_176 = 0.0f;
    s.time_in_state_180 = 0.0f;
    s.start_y_184 = 0.0f;
    s.max_y_188 = 0.0f;
    s.com_max_y_192 = 0.0f;
    s.collision_time_196 = 1.0f;
    s.time_to_apex_200 = 0.0f;
    s.body_flip_target_speed_204 = 0.0f;
    s.reached_apex_208 = false;
    s.landing_heading_valid_209 = false;
    s.start_flipped_210 = false;
    s.body_flipping_211 = false;
    s.grind_air_adjust_activated_212 = false;
    s.targeting_grind_213 = false;
    s.trajectory_index_216 = 0;
    return s;
}

inline KnownAirFrame frame() {
    KnownAirFrame f;
    f.start_flip_reference_96 = {};
    f.alternate_head_target_112 = {};
    f.velocity_400 = {};
    f.ground_normal_464 = {0.0f, 1.0f, 0.0f, 0.0f};
    f.start_height_484 = 0.0f;
    f.skater_up_544 = {0.0f, 1.0f, 0.0f, 0.0f};
    f.flags_2468 = 0;
    f.flags_2472 = 0;
    f.flags_2476 = 0;
    f.flags_2480 = 0;
    f.flags_2484 = 0;
    f.flags_2488 = 0;
    f.next_physics_state_2500 = 0;
    f.delta_time_2604 = 1.0f / 60.0f;
    f.forward_speed_2612 = 0.0f;
    f.body_spin_input_2640 = 0.0f;
    f.selector_landing_normal_2656 = {0.0f, 1.0f, 0.0f, 0.0f};
    return f;
}

inline KnownAirSettings settings() {
    KnownAirSettings s;
    s.max_heading_adjust_vs_up_y_160 = flat_graph(180.0f);
    s.landing_speed_scalar_vs_ground_normal_y_240 = flat_graph(1.0f);
    s.flip_start_collision_time_vs_normal_y = flat_graph(10.0f);
    s.trajectory_error_blend_away_time_384 = 1.0f;
    s.min_target_heading_velocity_420 = 0.1f;
    s.min_auto_body_speed_424 = 0.01f;
    s.max_spin_speed_428 = 360.0f;
    s.frames_for_grind_air_assist_436 = 4.0f;
    s.body_flip_min_grab_time_fraction_456 = 0.25f;
    s.flip_scalar = 1.0f;
    return s;
}

inline KnownAirModeSettings mode() {
    KnownAirModeSettings m;
    m.upside_down_falling_wipeout_enabled_2 = true;
    m.perfect_body_flips_28 = false;
    m.body_spin_speed_limit_56 = 20.0f;
    return m;
}

inline KnownAirReckoningFields reckoning() {
    KnownAirReckoningFields r;
    r.landing_normal_1152 = {0.0f, 1.0f, 0.0f, 0.0f};
    r.heading_axis_1200 = {1.0f, 0.0f, 0.0f, 0.0f};
    r.body_spin_angle_1568 = 0.0f;
    r.body_spin_speed_1572 = 0.0f;
    return r;
}

inline KnownAirOutput output() {
    KnownAirOutput o;
    o.selected_trajectory_240 = trajectory();
    return o;  // every other field is zero/false, as in mock.rs
}

class MockRuntime final : public KnownAirRuntime {
public:
    std::vector<std::string> calls;
    KnownAirPrediction prediction{trajectory(), 1.0f, 2};
    KnownAirTrajectory selector_trajectory = trajectory();
    Vector4 contact{};
    Vector4 selector_vector{};
    Vector4 selector_com{};
    std::pair<Vector4, float> highest{Vector4{}, 0.0f};
    float board_y = 0.0f;
    Vector4 com{};
    bool targeting_grind = false;
    std::pair<std::int32_t, Vector4> closest{0, Vector4{}};
    bool grind_started = false;
    bool grind_adjusting = false;
    Vector4 board_velocity{};
    Vector4 written_board_velocity{};
    Vector4 forward{1.0f, 0.0f, 0.0f, 0.0f};
    float signed_angle = 0.0f;
    RestoreVelocityGeometry restore_geometry{Vector4{}, 0.0f, 0.0f};

    static MockRuntime new_() { return MockRuntime{}; }

private:
    static std::string b(bool v) { return v ? "true" : "false"; }
    void call(std::string value) { calls.push_back(std::move(value)); }

public:
    // KnownAirMath
    float dot3(Vector4 left, Vector4 right) override {
        return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
    }
    Vector4 divide_vector_by_scalar(Vector4 vector, float scalar) override {
        return {vector[0] / scalar, vector[1] / scalar, vector[2] / scalar, vector[3] / scalar};
    }
    std::pair<Vector4, float> normalize_safe_with_length(Vector4 vector) override {
        const float length =
            s3::rs::sqrt(vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2]);
        if (length == 0.0f) {
            return {Vector4{}, 0.0f};
        }
        return {Vector4{vector[0] / length, vector[1] / length, vector[2] / length, vector[3] / length}, length};
    }
    float signed_angle_about_axis(Vector4, Vector4, Vector4) override {
        call("math.signed_angle");
        return signed_angle;
    }
    float wrap_angle_8258db98(float angle) override {
        call("math.wrap");
        return angle;
    }
    RestoreVelocityGeometry restore_velocity_geometry_82d35998(Vector4, Vector4) override {
        call("math.restore_geometry");
        return restore_geometry;
    }
    Vector4 transform_point_82d36880(AffineTransform, Vector4 point) override { return point; }

    // KnownAirRuntime
    void enable_board_angular_drive_only() override { call("board.angular_only"); }
    void set_air_collision_update_enabled(bool value) override { call("collision.enabled:" + b(value)); }
    void set_skeleton_collision_state(std::uint32_t value) override {
        call("collision.state:" + std::to_string(value));
    }
    void set_skeleton_physics_truck_tilt_enabled(bool value) override { call("skeleton.truck_tilt:" + b(value)); }
    void set_skeleton_inverse_kinematics_enabled(bool value) override { call("skeleton.ik:" + b(value)); }
    void reset_reckoning_flipping() override { call("reckoning.reset_flip"); }
    void set_footplant_flag_240(bool value) override { call("footplant.flag:" + b(value)); }
    void reset_footplants() override { call("footplant.reset"); }
    float board_transform_height() override {
        call("board.height");
        return board_y;
    }
    Vector4 skeleton_com_position() override {
        call("skeleton.com");
        return com;
    }
    bool selector_targeting_grind() const override { return targeting_grind; }
    void start_grind_air_adjust_from_selector() override { call("grind.start"); }
    std::pair<std::int32_t, Vector4> selector_closest_trajectory_point(Vector4, Vector4) override {
        call("selector.closest");
        return closest;
    }
    KnownAirPrediction selected_prediction() const override { return prediction; }
    KnownAirTrajectory selector_trajectory_2704() const override { return selector_trajectory; }
    Vector4 selector_contact_position() override {
        call("selector.contact");
        return contact;
    }
    Vector4 selector_vector_2848() const override { return selector_vector; }
    Vector4 selector_com_position_2784() const override { return selector_com; }
    std::pair<Vector4, float> highest_trajectory_position(const KnownAirTrajectory&) override {
        call("trajectory.highest");
        return highest;
    }
    bool selector_has_just_changed() const override { return false; }
    void reset_trajectory_selector() override { call("selector.reset"); }
    void begin_reckoning_body_flip(bool side) override { call("reckoning.begin_flip:" + b(side)); }
    void update_air_collision() override { call("collision.update"); }
    bool grind_air_adjust_started() const override { return grind_started; }
    void set_grind_air_adjust_started(bool value) override { call("grind.started:" + b(value)); }
    void set_grind_air_adjust_activated(bool value) override { call("grind.activated:" + b(value)); }
    bool grind_air_adjust_adjusting() const override { return grind_adjusting; }
    void disable_grind_foot_collision(GrindFootBody body) override {
        const char* name = "";
        switch (body) {
            case GrindFootBody::Body3152: name = "3152"; break;
            case GrindFootBody::Body3156: name = "3156"; break;
            case GrindFootBody::Body3160: name = "3160"; break;
            case GrindFootBody::Body3168: name = "3168"; break;
            case GrindFootBody::Body3172: name = "3172"; break;
            case GrindFootBody::Body3176: name = "3176"; break;
        }
        call(std::string("grind.disable:") + name);
    }
    void update_reckoning_air_states(Vector4, float, float, float) override { call("reckoning.update_air"); }
    void update_known_air_skeleton(Vector4) override { call("skeleton.update_known_air"); }
    void set_skeleton_add_skateboard_error(bool value) override { call("skeleton.board_error:" + b(value)); }
    void set_head_tracking_target(Vector4, bool valid) override { call("head.target:" + b(valid)); }
    void update_board_steering_tilt(float) override { call("board.steering_zero"); }
    void shift_footplant_trajectory_to_index(KnownAirTrajectory&, std::int32_t) override { call("footplant.shift"); }
    void update_footplant_prediction(const KnownAirFootplantInput&) override { call("footplant.prediction"); }
    void update_footplant_lock(const KnownAirFootplantInput&) override { call("footplant.lock"); }
    void update_footplant_pose(const KnownAirFootplantInput&) override { call("footplant.pose"); }
    Vector4 board_body_velocity() override {
        call("board.velocity");
        return board_velocity;
    }
    void set_board_velocity(Vector4 value) override {
        call("board.set_velocity");
        written_board_velocity = value;
    }
    Vector4 board_forward_axis() override {
        call("board.forward");
        return forward;
    }
    void check_for_air_wipeout(bool value) override { call("wipeout.check:" + b(value)); }
    AffineTransform reckoning_com_transform_816() const override { return AffineTransform{}; }
};

}  // namespace air_known_mock
