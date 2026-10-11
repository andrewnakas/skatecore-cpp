// Port of skate-core/src/physics/grind_contact/entry_tests.rs.
#include "harness.hpp"
#include "s3/core/physics/grind_contact/entry.hpp"

namespace {
using namespace s3::physics::grind_contact;
using namespace s3::physics::grind_contact::entry;
using s3::point_graph::PointGraph;

const PointGraph<4> HELP{{-1.0f, 0.0f, 0.5f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}};
Input input() {
    Input i;
    i.valid = true;
    i.kind = 0;
    i.category = 200;
    i.previous_state_2504 = 200;
    i.speed = 4.0f;
    i.balance_2720 = 0.0f;
    i.direction = {1.0f, 0.0f, 0.0f, 0.0f};
    i.normal = {0.0f, 1.0f, 0.0f, 0.0f};
    i.up = {0.0f, 1.0f, 0.0f, 0.0f};
    i.board_velocity = {4.0f, -2.0f, 3.0f, 0.0f};
    i.air_velocity = {4.0f, -2.0f, 3.0f, 0.0f};
    i.surface_kind = 0;
    i.high_side = {0.0f, 0.0f, 1.0f, 0.0f};
    i.vertical_help = &HELP;
    i.max_delta = 100.0f;
    i.flags = 0;
    i.previous_entry_velocity = {9.0f, 9.0f, 9.0f, 9.0f};
    return i;
}
using Reasons = std::vector<std::size_t>;

S3_TEST(ground_and_air_use_different_velocity_projections) {
    Engagement state;
    const Output air = state.update(input());
    Input g = input();
    g.category = 100;
    const Output ground = state.update(g);
    CHECK(s3::rs::abs(air.entry_velocity[1] + 2.0f) < 0.00001f);
    CHECK(s3::rs::abs(air.entry_velocity[2] - 1.8f) < 0.00001f);
    CHECK(s3::rs::abs(ground.entry_velocity[1] + 1.6f) < 0.00001f);
    CHECK(s3::rs::abs(ground.entry_velocity[2] - 2.4f) < 0.00001f);
}

S3_TEST(steep_ground_entry_rejects_without_air_wipeout) {
    Input steep = input();
    steep.up = {1.0f, 0.0f, 0.0f, 0.0f};
    Engagement state;
    const Output air = state.update(steep);
    CHECK(!air.valid);
    CHECK_EQ(air.wipeout_reasons, (Reasons{13}));
    Input g = steep;
    g.category = 100;
    const Output ground = state.update(g);
    CHECK(!ground.valid);
    CHECK(ground.wipeout_reasons.empty());
    CHECK_EQ(ground.flags & 0x40000000u, 0u);
}

S3_TEST(both_impact_requests_survive_first_rejection_including_on_ground) {
    Engagement state;
    Input i = input();
    i.category = 100;
    i.max_delta = 0.0f;
    i.board_velocity = {4.0f, 0.0f, 8.0f, 0.0f};
    const Output result = state.update(i);
    CHECK(!result.valid);
    CHECK_EQ(result.wipeout_reasons, (Reasons{9, 14}));
}

S3_TEST(tipslide_latch_counts_down_during_invalid_investigations) {
    Engagement state;
    Input i = input();
    i.kind = 2;
    i.previous_state_2504 = 403;
    const Output result = state.update(i);
    CHECK_NE(result.flags & 0x02000000u, 0u);
    CHECK_EQ(state.tipslide_frames, 8u);
    for (int remaining = 7; remaining >= 0; --remaining) {
        Input inv = input();
        inv.valid = false;
        const Output r = state.update(inv);
        CHECK_EQ(state.tipslide_frames, static_cast<std::uint32_t>(remaining));
        CHECK_EQ(r.entry_velocity, (V{9.0f, 9.0f, 9.0f, 9.0f}));
        CHECK_EQ(r.impact_speed, 0.0f);
    }
    Input inv = input();
    inv.valid = false;
    state.update(inv);
    CHECK_EQ(state.tipslide_frames, 0u);
}

S3_TEST(coping_and_entry_slope_dead_zones_are_distinct) {
    const V velocity{1.0f, 0.005f, 0.0f, 0.0f};
    const V tangent{1.0f, 0.0f, 0.0f, 0.0f};
    CHECK_EQ(admission::approach_slope_sine(tangent, velocity), 0.0f);
    CHECK(s3::rs::abs(admission::engagement_slope_sine(tangent, velocity) - 1.0f) < 0.00001f);
}
}  // namespace
