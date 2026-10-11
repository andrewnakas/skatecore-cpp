// Port of skate-core/src/player/offboard/slide/tests.rs.
#include "harness.hpp"
#include "s3/core/player/offboard/slide.hpp"

namespace {
using namespace s3::player::offboard::slide;
using s3::point_graph::PointGraph;

PointGraph<8> constant(float value) {
    PointGraph<8> g;
    g.x = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f};
    g.y.fill(value);
    return g;
}
Input input() { return Input{false, {0.0f, 1.0f, 0.0f, 0.0f}, Vector{}, std::nullopt}; }
void close(float a, float b) { CHECK_MSG(std::fabs(a - b) < 1e-6f, a << " != " << b); }

S3_TEST(special_mode_retains_elapsed_until_low_heading_allows_reentry) {
    SpecialMode mode{false, 0.0f};
    mode.update(1.0f, 0x200, 0.0f);
    CHECK(mode.enabled_714);
    mode.update(0.0f, 0, 0.5f);
    CHECK(!mode.enabled_714);
    CHECK(mode.elapsed_788 > 0.0f);
    mode.update(1.0f, 0x200, 0.5f);
    CHECK(!mode.enabled_714);
    mode.update(1.0f, 0x200, 0.0f);
    CHECK_EQ(mode.elapsed_788, 0.0f);
    CHECK(!mode.enabled_714);
    mode.update(1.0f, 0x200, 0.0f);
    CHECK(mode.enabled_714);
}

S3_TEST(special_mode_skips_both_slide_fields) {
    Sliding slide{{1.0f, 2.0f, 3.0f, 0.0f}, true};
    const auto prior = slide;
    auto i = input();
    i.special_mode_714 = true;
    slide.update(i, constant(1.0f), constant(1.0f));
    CHECK(slide == prior);
}

S3_TEST(flat_surface_decays_old_slide_without_downhill_force) {
    Sliding slide{{1.0f, 0.0f, 0.0f, 0.0f}, true};
    slide.update(input(), constant(1.0f), constant(1.0f));
    close(slide.velocity_528[0], 0.95f * 0.9f);
    CHECK(slide.active_710);
}

S3_TEST(slope_force_follows_surface_and_contact_removes_into_wall_component) {
    auto i = input();
    i.surface_normal_560 = {0.0f, 0.8f, 0.6f, 0.0f};
    Sliding slide{Vector{}, false};
    slide.update(i, constant(1.0f), constant(1.0f));
    CHECK(slide.velocity_528[1] < 0.0f);
    CHECK(slide.velocity_528[2] > 0.0f);
    CHECK(slide.active_710);
    i.contact_direction_400 = Vector{0.0f, 0.0f, -1.0f, 0.0f};
    slide.update(i, constant(1.0f), constant(1.0f));
    close(slide.velocity_528[2], 0.0f);
}
}  // namespace
