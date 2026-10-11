// Port of skate-core/src/player/offboard/ground_lifecycle/tests.rs.
#include <vector>

#include "harness.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/player/offboard/ground_lifecycle.hpp"

namespace {
using namespace s3::player::offboard::ground_lifecycle;
using s3::physics::skeleton_animation_record::IDENTITY;
using s3::player::offboard::ground_entry::State;
using s3::player::wipeout::Frame;
using s3::player::wipeout::Requests;

Frame input() {
    Frame f;
    f.category = 100;
    f.timestep = 1.0f / 60.0f;
    f.time_on_ground = 1.0f;
    f.animation_up = {0.0f, 1.0f, 0.0f, 0.0f};
    f.jump_fix_frames = 1000;
    f.deck = IDENTITY;
    f.input_board = IDENTITY;
    f.world_to_animation = IDENTITY;
    f.board_contact = true;
    f.wheel_contact = true;
    f.board_contact_normal = {0.0f, 1.0f, 0.0f, 0.0f};
    f.highest_normal = {0.0f, 1.0f, 0.0f, 0.0f};
    f.system_up_y = 1.0f;
    f.grind_normal = {0.0f, 1.0f, 0.0f, 0.0f};
    return f;
}
CollisionSettings settings() { return CollisionSettings{0.35f, 6.0f, 0.3f, 20.0f, 20.0f, 0.0f, 0.6f, 1.0f}; }
CollisionInput collision(const Frame& f) {
    CollisionInput c;
    c.shared = &f;
    c.skeleton_velocity_16336 = {1.0f, 0.0f, 0.0f, 0.0f};
    c.contact_flag_4072 = false;
    c.contact_force_4056 = 0.0f;
    return c;
}
PostInput post(const Frame& f) {
    PostInput p;
    p.collision = collision(f);
    return p;
}

S3_TEST(duplicate_ground_triggers_increment_count_and_keep_shared_requests) {
    const auto f = input();
    auto p = post(f);
    p.processed_flags_2484 = 0x400;
    State s;
    s.flags_144_to_150[6] = true;
    s.flags_144_to_150[2] = true;
    s.flags_144_to_150[1] = true;
    auto q = Requests::new_();
    q.request(3, 2.0f);
    post_physics(s, q, settings(), p);
    CHECK_EQ(q.count, 4u);
    CHECK(q.reasons[31] && q.reasons[28] && q.reasons[3]);
    CHECK_EQ(q.values[3], 2.0f);
    CHECK_EQ(q.mode, 4u);
}

S3_TEST(displacement_requires_persistent_ticks_then_clears_when_contact_settles) {
    const auto f = input();
    auto p = post(f);
    p.skeleton_displacement_16304 = {0.0f, 0.5f, 0.0f, 0.0f};
    State s;
    auto q = Requests::new_();
    for (int i = 0; i < 2; ++i) post_physics(s, q, settings(), p);
    CHECK_EQ(q.count, 0u);
    post_physics(s, q, settings(), p);
    CHECK(q.reasons[32]);
    p.skeleton_displacement_16304 = {};
    post_physics(s, q, settings(), p);
    CHECK_EQ(s.elapsed_168, 0.0f);
}

S3_TEST(opposite_displacement_triggers_only_beyond_speed_scaled_threshold) {
    const auto f = input();
    auto p = post(f);
    p.processed_flags_2484 = 0x400;
    p.processed_velocity_608 = {2.0f, 0.0f, 0.0f, 0.0f};
    State s;
    auto q = Requests::new_();
    p.skeleton_displacement_16304 = {-0.02f, 0.0f, 0.0f, 0.0f};
    post_physics(s, q, settings(), p);
    CHECK_EQ(q.count, 0u);
    p.skeleton_displacement_16304[0] = -0.03f;
    post_physics(s, q, settings(), p);
    CHECK(q.reasons[32]);
}

S3_TEST(vehicle_scalar_applies_even_below_vehicle_request_force) {
    auto f = input();
    f.maximum_pose_error = 0.3f;
    auto i = collision(f);
    i.contact_flag_4072 = true;
    i.contact_force_4056 = 0.0f;
    auto q = Requests::new_();
    check_collision(q, settings(), i);
    CHECK(q.reasons[18]);
    CHECK(!q.reasons[7]);
}

S3_TEST(collision_priority_selects_squash_before_displacement_and_force) {
    auto f = input();
    f.maximum_pose_error = 1.0f;
    f.pose_error = {1.0f, 0.0f, 0.0f, 0.0f};
    f.regions_force.fill(100.0f);
    auto q = Requests::new_();
    check_collision(q, settings(), collision(f));
    CHECK(q.reasons[18]);
    CHECK(!q.reasons[1] && !q.reasons[0]);
}

PublicationInput publication() {
    PublicationInput p;
    p.contact_position_192 = {3.0f, 0.0f, 4.0f, 0.0f};
    p.ground_kind_356 = 4;
    p.ground_scalar_360 = 0.7f;
    p.motion_vector_1040 = {2.0f, 3.0f, 4.0f, 5.0f};
    p.motion_up_864 = {0.0f, 1.0f, 0.0f, 2.0f};
    p.hand_flags = {{{true, false, false}, {true, true, false}}};
    return p;
}

S3_TEST(output_gates_distance_and_preserves_projection_w_lane) {
    State s;
    s.distance_164 = 0.0f;
    auto i = publication();
    auto o = publish(s, i);
    CHECK_EQ(o.offboard_distance_116, 1e10f);
    CHECK(o.offboard_flag_330);
    CHECK_EQ(o.animation_vector_144, (Vector{2.0f, 0.0f, 4.0f, -1.0f}));
    CHECK_EQ(o.offboard_hand_flags_306_307, (std::array<bool, 2>{true, false}));
    i.ground_flags_752_to_754[0] = true;
    i.contact_flags_368 = 9;
    o = publish(s, i);
    CHECK_EQ(o.offboard_distance_116, 5.0f);
    CHECK(o.offboard_flag_329);
    CHECK(!o.offboard_flag_330);
}

S3_TEST(exit_calls_real_refresh_before_clearing_and_releasing) {
    struct Service : ExitServices {
        std::vector<int> calls;
        bool active = true;
        void refresh_toolkit_contacts() override { calls.push_back(1); }
        void clear_toolkit_exit_fields() override { calls.push_back(2); }
        bool ground_geometry_active() const override { return active; }
        void release_ground_geometry() override { calls.push_back(3); }
        void clear_ground_geometry_active() override {
            calls.push_back(4);
            active = false;
        }
    };
    Service service;
    exit(service);
    CHECK_EQ(service.calls, (std::vector<int>{1, 2, 3, 4}));
    service.calls.clear();
    exit(service);
    CHECK_EQ(service.calls, (std::vector<int>{1, 2}));
}
}  // namespace
