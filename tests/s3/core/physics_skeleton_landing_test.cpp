// Port of skate-core/src/physics/tests/skeleton_landing.rs.
#include "harness.hpp"
#include "s3/core/physics/skeleton_landing.hpp"

namespace {
using namespace s3::physics::skeleton_landing;

LandingSettings settings() {
    PointGraph<4> graph;
    graph.x = {0.0f, 0.3f, 0.6f, 1.0f};
    graph.y = {1.0f, 1.0f, 1.0f, 1.0f};
    LandingSettings s;
    s.manual_blend = graph;
    s.grind_blend = graph;
    s.coffin_height = graph;
    s.minimum_height = 0.5f;
    s.maximum_velocity = 8.0f;
    s.manual_damping = 17.5f;
    s.manual_spring = 60.0f;
    s.ground_minimum_compression_time = 0.05f;
    s.ground_damping = 6.0f;
    s.ground_spring = 60.0f;
    s.grind_animation_target_time = 0.15f;
    s.grind_damping = 10.0f;
    s.grind_spring = 60.0f;
    s.grind_target_delta = 0.01f;
    s.desired_com_height = 0.85f;
    s.coffin_time = 0.04f;
    s.coffin_maximum_velocity = 5.0f;
    s.coffin_blend_frames = 7.0f;
    s.coffin_base_height = 0.2f;
    return s;
}

S3_TEST(ground_landing_uses_physical_height_velocity_and_preserves_release_order) {
    LandingAdjustment adjustment = LandingAdjustment::default_();
    SkateboardOffset offset = SkateboardOffset::default_();
    LandingInput input{2, 0, 0, 0, 0.0f, -2.0f, 0.8f, 1.0f};
    adjustment.update(input, settings(), offset);
    CHECK(!adjustment.active);
    input.filtered_state = 1;
    input.flags_2476 = 0x10000000u;
    adjustment.update(input, settings(), offset);
    CHECK(adjustment.active);
    CHECK_EQ(adjustment.kind, 0u);
    CHECK(s3::rs::abs(adjustment.velocity + 1.75f) < 1e-6f);
    CHECK(s3::rs::abs(offset.transform[3][1] - 0.205f) < 1e-6f);
    CHECK_EQ(offset.height_frames, 15.0f);
    CHECK(offset.height_refreshed);
    input.filtered_state = 2;
    adjustment.update(input, settings(), offset);
    CHECK(!adjustment.active);
    CHECK_EQ(adjustment.time, STEP + STEP);
    CHECK_EQ(adjustment.previous_filtered_state, 2u);
}
}  // namespace
