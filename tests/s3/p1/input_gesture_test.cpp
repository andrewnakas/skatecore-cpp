// Port of skate-core/src/input/gesture.rs (#[cfg(test)] mod tests).
#include "harness.hpp"
#include "s3/core/input/gesture.hpp"

namespace {
using namespace s3::input::gesture;

Recognizer recognizer() {
    return Recognizer::new_({Pattern{
                                "360Flip",
                                {
                                    {-0.965714f, 0.268571f},
                                    {-0.497143f, 0.84f},
                                    {0.211429f, 0.988571f},
                                    {0.908571f, -0.405714f},
                                },
                                0.4f * 0.4f,
                            }})
        .unwrap();
}
const Settings SETTINGS = Settings{3, 0};

S3_TEST(anticipation_hold_does_not_weaken_authored_scoop) {
    auto r = recognizer();
    const auto points = r.patterns()[0].points;
    r.sample({0.0f, 0.0f}, SETTINGS);
    for (int i = 0; i < 120; ++i) {
        CHECK(!r.sample(points[0], SETTINGS).has_value());
    }
    CHECK(!r.sample(points[1], SETTINGS).has_value());
    CHECK(!r.sample(points[2], SETTINGS).has_value());
    const auto result = s3::rs::unwrap(r.sample(points[3], SETTINGS));
    CHECK_EQ(result.pattern, std::size_t(0));
    CHECK_EQ(result.strength, 1.0f);
    CHECK_EQ(result.elapsed, 4.0f);
    CHECK(r.held(points[3]) == std::optional<std::size_t>(0));
    CHECK(!r.sample(points[3], SETTINGS).has_value());
    CHECK(r.held({0.0f, 0.0f}) == std::nullopt);
}

S3_TEST(disconnected_scoop_and_reverse_path_do_not_trigger) {
    auto r = recognizer();
    const auto points = r.patterns()[0].points;
    r.sample({0.0f, 0.0f}, SETTINGS);
    r.sample(points[0], SETTINGS);
    for (int i = 0; i < 4; ++i) {
        CHECK(!r.sample({0.0f, 0.0f}, SETTINGS).has_value());
    }
    for (std::size_t i = 1; i < points.size(); ++i) {
        CHECK(!r.sample(points[i], SETTINGS).has_value());
    }
    auto r2 = recognizer();
    r2.sample({0.0f, 0.0f}, SETTINGS);
    for (auto it = points.rbegin(); it != points.rend(); ++it) {
        CHECK(!r2.sample(*it, SETTINGS).has_value());
    }
}
}  // namespace
