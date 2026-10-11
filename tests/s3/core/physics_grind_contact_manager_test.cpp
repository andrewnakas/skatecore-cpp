// Port of skate-core/src/physics/grind_contact/manager_tests.rs.
#include "harness.hpp"
#include "s3/core/physics/grind_contact/manager.hpp"

namespace {
using namespace s3::physics::grind_contact;
using namespace s3::physics::grind_contact::manager;
using s3::point_graph::PointGraph;

GeometryInput geometry() {
    GeometryInput g;
    g.valid = true;
    g.family = 2;
    g.current_state = 100;
    g.category = 100;
    g.air_frames = 0;
    g.geometry_kind = 2;
    g.geometry_flags = 0;
    g.far_points = {V{0.0f, 0.1f, 0.2f, 0.0f}, V{0.0f, -0.1f, -0.2f, 0.0f}};
    g.upmost = {0.0f, 1.0f, 0.0f, 0.0f};
    g.high_side = {0.0f, 0.0f, 1.0f, 0.0f};
    g.point = {0.0f, 0.0f, 0.0f, 0.0f};
    g.direction = {1.0f, 0.0f, 0.0f, 0.0f};
    g.board_position = {0.0f, 0.1f, 0.2f, 0.0f};
    g.board_forward = {1.0f, 0.0f, 0.0f, 0.0f};
    g.velocity = {1.0f, 0.0f, 0.0f, 0.0f};
    g.deck_to_truck = 0.243f;
    g.previous_exit_angle = 0.0f;
    g.previous_exit_direction = {1.0f, 0.0f, 0.0f, 0.0f};
    g.flags = 0;
    return g;
}

S3_TEST(backslash_uses_height_difference_and_preserves_existing_tipslide) {
    std::uint32_t counter = 0;
    CHECK_EQ(tweak_geometry(geometry(), counter).family, 4u);
    GeometryInput g = geometry();
    g.current_state = 402;
    CHECK_EQ(tweak_geometry(g, counter).family, 2u);
    g = geometry();
    g.board_position = {0.0f, 0.0f, -1.0f, 0.0f};
    CHECK_EQ(tweak_geometry(g, counter).family, 2u);
}

S3_TEST(five_o_hanging_deck_sets_pry_then_excludes_thirty_frames) {
    std::uint32_t counter = 0;
    GeometryInput input = geometry();
    input.family = 3;
    input.board_forward = {0.0f, -0.1f, 0.9f, 0.0f};
    const GeometryOutput result = tweak_geometry(input, counter);
    CHECK(!result.valid);
    CHECK_NE(result.flags & 0x10000000u, 0u);
    CHECK_EQ(counter, 30u);
    GeometryInput again = input;
    again.board_forward = {1.0f, 0.0f, 0.0f, 0.0f};
    CHECK(!tweak_geometry(again, counter).valid);
}

S3_TEST(coping_timer_is_reduced_on_activation_and_expires_without_contact) {
    const PointGraph<4> vertical{{-1.0f, 0.0f, 1.0f, 2.0f}, {2.0f, 2.0f, 2.0f, 2.0f}};
    const PointGraph<4> linear{{0.0f, 1.0f, 2.0f, 3.0f}, {0.5f, 0.5f, 0.5f, 0.5f}};
    float timer = 0.0f;
    const V tangent{1.0f, 0.0f, 0.0f, 0.0f};
    const V velocity{3.0f, 1.0f, 0.0f, 0.0f};
    CHECK_EQ(gravity_relief(timer, true, 100, tangent, velocity, 0.25f, vertical, linear), 0.75f);
    CHECK_EQ(gravity_relief(timer, false, 200, tangent, velocity, 1.0f, vertical, linear), 0.0f);
}

S3_TEST(jumper_retains_geometry_and_regenerates_per_update_not_per_second) {
    Jumper jumper = Jumper::default_();
    jumper.cooldown = 2;
    jumper.energy = 0.4f;
    CHECK_EQ(jumper.update(std::nullopt, UINT32_MAX), 0x02000000u);
    CHECK_EQ(jumper.family, 3u);
    CHECK_EQ(jumper.geometry.normal, (V{0.0f, 1.0f, 0.0f, 0.0f}));
    CHECK_EQ(jumper.energy, 0.4f + 0.0035f);
    CHECK_EQ(jumper.update(std::nullopt, 5), 0u);
    CHECK_EQ(jumper.family, 5u);
}

S3_TEST(directed_rail_retains_previous_sign_only_at_low_speed) {
    const V start{0.0f, 0.0f, 0.0f, 0.0f};
    const V end{1.0f, 0.0f, 0.0f, 0.0f};
    const V previous{-1.0f, 0.0f, 0.0f, 0.0f};
    CHECK(directed_tangent(start, end, {0.05f, 0.0f, 0.0f, 0.0f}, previous)[0] < 0.0f);
    CHECK(directed_tangent(start, end, {0.1f, 0.0f, 0.0f, 0.0f}, previous)[0] > 0.0f);
}
}  // namespace
