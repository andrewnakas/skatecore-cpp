// Port of the inline tests in skate-core/src/riding/ground_contact_response.rs.
#include "harness.hpp"
#include "s3/core/riding/ground_contact_response.hpp"

namespace {
using namespace s3::riding::ground_contact_response;
using s3::math::Vector3;
using s3::point_graph::PointGraph;

S3_TEST(contact_force_and_auto_jump_have_distinct_gates_and_keep_old_velocity) {
    WallRideSettings settings;
    settings.anti_gravity_vs_time = PointGraph<8>{{0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f},
                                                  {0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f}};
    settings.max_dot_floor_wall = 0.5f;
    settings.foot_force_time = 0.09f;
    settings.auto_jump_height = 0.6f;
    settings.max_time = 1.f;
    settings.velocity_time_to_consider = 0.25f;
    settings.auto_jump_y_down_scalar = 0.66f;
    settings.auto_jump_force = 3.f;
    WallRidePhysical physical;
    physical.board_normal = {1.f, 0.f, 0.f, 0.f};
    physical.up = {1.f, 0.f, 0.f, 0.f};
    physical.velocity = {2.f, -2.f, 4.f, 0.f};
    physical.board_mass = 8.f;
    physical.gravity = 9.81f;
    physical.speed = 5.f;
    physical.contact_count = 0;
    GroundContactFrame frame;
    frame.vector_8032 = {0.f, 0.f, 0.f, 0.f};
    frame.vector_8048 = {0.f, 0.2f, 0.f, 0.f};
    frame.vector_8064 = {0.f, 1.f, 0.f, 0.f};
    frame.word_8080 = 0;
    frame.flag_8084 = true;
    frame.scalar_2752 = 0.f;
    frame.scalar_2756 = 0.f;
    const std::array<float, 4> old{9.f, 9.f, 9.f, 9.f};
    const auto force = wall_ride_response(settings, physical, frame, old);
    CHECK(force.active_2731);
    CHECK(!force.animated_board_2708);
    CHECK_EQ(force.vector_2688, old);
    CHECK(s3::rs::abs(force.tag_16_force.force_world.y + 7.848f) < 0.00001f);
    frame.scalar_2752 = 0.1f;
    const auto launch = wall_ride_response(settings, physical, frame, old);
    CHECK(launch.animated_board_2708);
    CHECK_EQ(launch.vector_2688, (std::array<float, 4>{3.f, -1.32f, 4.f, 0.f}));
    frame.vector_8064 = physical.board_normal;
    CHECK(!wall_ride_response(settings, physical, frame, old).animated_board_2708);
    frame.flag_8084 = false;
    const auto no_contact = wall_ride_response(settings, physical, frame, old);
    CHECK(!no_contact.active_2731);
    CHECK_EQ(no_contact.tag_16_force.force_world, Vector3::ZERO);
}

}  // namespace
