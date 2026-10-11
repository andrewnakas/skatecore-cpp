// Port of the #[cfg(test)] module in skate-core/src/animation/ground_acceleration.rs.
#include "harness.hpp"
#include "s3/core/animation/ground_acceleration.hpp"

using namespace s3::animation::ground_acceleration;

S3_TEST(pitched_deck_is_projected_before_the_distinct_ground_frame) {
    // Deck Y points world Z. Ground X points world Z. World Z is removed
    // before changing frame; removing only final Y would incorrectly keep it.
    const AnimationPartTransform deck = {{
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, -1.0f, 0.0f, 0.0f},
        {7.0f, 8.0f, 9.0f, 0.0f},
    }};
    const AnimationPartTransform ground = {{
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {-1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 0.0f},
    }};
    const auto result = condition(Input{deck, ground, {2.0f, 3.0f, 4.0f, 0.0f}});
    CHECK_EQ((std::array<float, 3>{result[0], result[1], result[2]}), (std::array<float, 3>{0.0f, 0.0f, -2.0f}));
}

S3_TEST(bump_uses_only_scaled_horizontal_magnitude_and_strict_threshold) {
    const Settings settings{2.0f, 10.0f};
    CHECK(!is_bumped({0.0f, 0.0f, 0.0f, 0.0f}, settings));
    CHECK(!is_bumped({0.0f, 0.0f, 10.0f, 0.0f}, settings));
    CHECK(is_bumped({6.0f, 0.0f, 0.0f, 0.0f}, settings));
    CHECK(!is_bumped({0.0f, 0.0f, 0.0f, 999.0f}, settings));
}
