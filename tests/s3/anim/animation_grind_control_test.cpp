// Port of skate-core/src/animation/grind_control/tests.rs.
#include <tuple>

#include "harness.hpp"
#include "s3/core/animation/grind_control.hpp"

using namespace s3::animation::grind_control;

namespace {
FadeSettings settings() { return FadeSettings{1.0f, 1.0f, 0.25f, 0.5f}; }
}  // namespace

S3_TEST(fade_skips_first_update_then_accelerates_and_reverses_without_dt_scaling) {
    Fade a{};
    CHECK_EQ(a.begin(-1.0f, 1.0f, 0.0f), 0.0f);
    CHECK(a.update(10.0f, 1.0f, settings()) == std::nullopt);
    CHECK_EQ(a.elapsed, 0.0f);
    Fade b = a;
    CHECK(a.update(0.01f, 1.0f, settings()) == std::optional<float>(0.25f));
    CHECK(b.update(0.5f, 1.0f, settings()) == std::optional<float>(0.25f));
    CHECK_NE(a.elapsed, b.elapsed);
    CHECK(a.update(0.01f, 1.0f, settings()) == std::optional<float>(0.75f));
    CHECK(a.update(0.01f, -1.0f, settings()) == std::optional<float>(1.0f));
    CHECK_MSG(a.step == 0.25f, "reversal must decelerate the retained step");
    CHECK(a.update(0.01f, -1.0f, settings()) == std::optional<float>(1.0f));
    CHECK(a.update(0.01f, -1.0f, settings()) == std::optional<float>(0.75f));
}

S3_TEST(fade_target_accumulates_and_begin_discards_previous_run) {
    Fade a{};
    a.begin(-2.0f, 2.0f, 9.0f);
    CHECK(std::make_tuple(a.value, a.target) == std::make_tuple(2.0f, 2.0f));
    a.update(1.0f, 0.0f, settings());
    a.update(1.0f, -0.25f, settings());
    CHECK_EQ(a.target, 1.75f);
    a.update(1.0f, -0.25f, settings());
    CHECK_EQ(a.target, 1.5f);
    a.begin(-1.0f, 1.0f, -9.0f);
    CHECK(std::make_tuple(a.value, a.target, a.step, a.elapsed) == std::make_tuple(-1.0f, -1.0f, 0.0f, 0.0f));
    CHECK(a.just_began);
}

S3_TEST(crouch_obeys_physical_minimum_and_two_sided_rate_limit) {
    CHECK_EQ(crouch(1.0f, 0.0f, 0.75f, 0.25f, 1.0f, 2.0f, 0.125f), 0.75f);
    CHECK_EQ(crouch(0.75f, 0.0f, 0.75f, 0.25f, 1.0f, 2.0f, 0.125f), 0.5f);
    CHECK_EQ(crouch(0.25f, 0.0f, 0.0f, 0.25f, 1.0f, 2.0f, 0.125f), 0.5f);
    CHECK_EQ(crouch(1.0f, 0.5f, 0.0f, 0.25f, 1.0f, 10.0f, 1.0f), 0.5f);
}

S3_TEST(physical_twist_is_mirrored_exactly_once) {
    const float pi = s3::rs::f32::consts::PI;
    CHECK_EQ(mirrored_twist(0.25f, false), 0.25f);
    CHECK_EQ(mirrored_twist(0.25f, true), pi - 0.25f);
    CHECK_EQ(mirrored_twist(-0.25f, true), -pi + 0.25f);
}

S3_TEST(facing_uses_cross_y_and_both_flags_including_zero_boundary) {
    for (bool ground : {false, true}) {
        for (bool mirror : {false, true}) {
            CHECK_EQ(facing::backwards({1.0f, 8.0f, 0.0f}, {0.0f, 3.0f, -1.0f}, ground, mirror),
                     bool(true ^ ground ^ mirror));
            CHECK_EQ(facing::backwards({1.0f, 8.0f, 0.0f}, {0.0f, 3.0f, 1.0f}, ground, mirror), bool(ground ^ mirror));
            CHECK_EQ(facing::backwards({0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, ground, mirror), bool(ground ^ mirror));
        }
    }
    // The other cross-product term is not interchangeable with a board Z test.
    CHECK(facing::backwards({0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, false, false));
}
