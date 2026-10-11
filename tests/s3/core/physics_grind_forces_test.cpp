// Port of skate-core/src/physics/grind_forces/tests.rs.
#include "harness.hpp"
#include "s3/core/physics/grind_forces.hpp"
#include "s3/core/physics/grind_forces/noise.hpp"
#include "s3/core/physics/grind_forces/release.hpp"
#include "s3/core/physics/grind_forces/slide.hpp"

namespace {
using namespace s3::physics::grind_forces;
using s3::physics::native_arithmetic::dot3;

const V X = {1.0f, 0.0f, 0.0f, 0.0f};
const V Y = {0.0f, 1.0f, 0.0f, 0.0f};
const V Z = {0.0f, 0.0f, 1.0f, 0.0f};
const V O = {0.0f, 0.0f, 0.0f, 0.0f};

void near(float a, float b) { CHECK_MSG(s3::rs::abs(a - b) < 0.0001f, a << " != " << b); }

S3_TEST(material_and_geometry_are_independent_and_support_is_not_the_grind_normal) {
    CHECK_EQ(material_multiplier(13), material_multiplier(2));
    CHECK_EQ(material_multiplier(999), 1.3f);
    const std::pair<std::uint32_t, float> cases[] = {{0, 50.0f}, {1, 40.0f}, {2, 30.0f}, {99, 30.0f}};
    for (const auto& [kind, strength] : cases) {
        const V force = friction(V{4.0f, 9.0f, 0.0f, 0.0f}, Y, V{0.0f, 0.25f, 0.0f, 0.0f}, 0.5f, true,
                                 material_multiplier(4), kind, {50.0f, 40.0f, 30.0f});
        near(force[0], -0.25f * 0.5f * 1.9f * 0.6f * strength * 0.48f);
        near(force[1], 0.0f);
    }
    CHECK(!friction_applies(V{0.001f, 10.0f, 0.0f, 0.0f}, Y));
    CHECK(friction_applies(V{0.002f, 10.0f, 0.0f, 0.0f}, Y));
}

S3_TEST(pin_uses_signed_reference_and_skips_zero_error_instead_of_waking_body) {
    CHECK(lateral_pin({X, Y, Z, O}, O, X, X, 800.0f, 0.0f, 0.0f, true, 1.0f) == std::nullopt);
    const V force = s3::rs::unwrap(
        lateral_pin({X, Y, Z, O}, X, X, V{2.0f, 0.0f, 0.0f, 0.0f}, 100.0f, 0.0f, 0.0f, true, 0.5f));
    near(force[0], (100.0f - 100.0f * 0.133f * 2.0f) * 0.5f);
    const s3::point_graph::PointGraph<4> graph{
        {-1.0f, 0.0f, 0.5f, 1.0f},
        {0.0f, 0.0f, 0.5f, 0.75f},
    };
    CHECK_EQ(pin_slope(Y, Y, 0.0f, graph), 0.75f);
    CHECK_EQ(pin_slope(Y, Y, 0.01f, graph), 1.0f);
}

slide::Input slide_input() {
    return slide::Input{O, O, Z, X, V{2.0f, 3.0f, 4.0f, 0.0f}, 0.5f, 10.0f, 30.0f, false, 0};
}

S3_TEST(slide_force_order_mass_frequency_and_darkslide_translation) {
    slide::Input input = slide_input();
    auto f = slide::control(slide::Slide::Boardslide, input);
    CHECK_EQ(f, (std::vector<V>{V{12.5f, 0.0f, 0.0f, 0.0f}, V{-40.0f, 0.0f, 0.0f, 0.0f}}));
    input.position[0] = 0.08f;
    f = slide::control(slide::Slide::Darkslide, input);
    near(f.at(0)[0], 10.0f);  // (.5 remaining)*40*.5 translation.
    input.position[0] = 0.2f;
    f = slide::control(slide::Slide::Boardslide, input);
    near(f.at(0)[0], -10.0f);
    near(f.at(1)[0], -600.0f);  // Actual input frequency30, not ZIP fixed60.
    near(f.at(1)[1], 0.0f);
    input.preparing_jump = true;
    input.position[0] = 0.12f;
    CHECK(slide::control(slide::Slide::Boardslide, input).empty());
}

S3_TEST(exit_lift_flag_is_independent_and_speed_equality_skips_force) {
    release::Input input{X, O, X, Y, O, 2, Z, false, false, 90.0f, 0.75f, 125.0f};
    CHECK(release::force(input) == std::optional<V>(V{0.0f, 0.0f, -90.0f, 0.0f}));
    input.enable_lift = true;
    CHECK(release::force(input) == std::optional<V>(V{0.0f, 125.0f, -90.0f, 0.0f}));
    input.force_across = true;
    input.velocity = V{0.75f, 0.0f, 0.0f, 0.0f};
    CHECK(release::force(input) == std::nullopt);
}

S3_TEST(noise_preserves_draw_order_modulo_speed_cap_and_position) {
    const auto a = noise::angles(2.0f, 1.0f, {0, 50000, 99999});
    near(a[0], -0.06f);
    near(a[1], 0.0f);
    near(a[2], 0.0599988f);
    CHECK_EQ(a, noise::angles(20.0f, 1.0f, {100000, 150000, 199999}));
    const std::array<V, 4> frame{X, Y, Z, V{8.0f, 9.0f, 10.0f, 1.0f}};
    const auto n = noise::apply(frame, 2.0f, 1.0f, {12345, 54321, 87654});
    CHECK_EQ(n[3], frame[3]);
    for (std::size_t c = 0; c < 3; ++c) {
        near(dot3(n[c], n[c]), 1.0f);
    }
    near(dot3(n[0], n[1]), 0.0f);
    CHECK_EQ(noise::apply(frame, 0.0f, 1.0f, {0, 1, 2}), frame);
}
}  // namespace
