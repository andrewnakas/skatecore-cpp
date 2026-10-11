// Port of the #[cfg(test)] module in skate-core/src/animation/bump.rs.
#include "harness.hpp"
#include "s3/core/animation/bump.hpp"

using namespace s3::animation::bump;

S3_TEST(bump_direction_scaling_thresholds_and_stance) {
    const Settings s{2.0f, 10.0f, 0.2f, 30.0f};
    CHECK_EQ(coefficients({0.0f, 0.0f, 0.0f, 0.0f}, false, s), (std::array<float, 2>{0.0f, 0.0f}));
    const std::pair<std::array<float, 4>, std::array<float, 2>> cases[] = {
        {{5.0f, 0.0f, 0.0f, 99.0f}, {0.2f, 0.0f}},
        {{10.0f, 0.0f, 0.0f, 0.0f}, {0.6f, 0.0f}},
        {{0.0f, 0.0f, -40.0f, 0.0f}, {0.0f, -1.0f}},
        {{0.0000001f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    };
    for (const auto& [input, expected] : cases) {
        const auto result = coefficients(input, false, s);
        const auto mirrored = coefficients(input, true, s);
        for (std::size_t i = 0; i < 2; ++i) {
            CHECK(s3::rs::abs(result[i] - expected[i]) < 1e-6f);
            CHECK_EQ(mirrored[i], -result[i]);
        }
    }
    const auto diagonal = coefficients({12.0f, 0.0f, 18.0f, 0.0f}, false, s);
    CHECK(s3::rs::abs(diagonal[0] - 0.8f) < 1e-6f && s3::rs::abs(diagonal[1] - 0.6f) < 1e-6f);
}
