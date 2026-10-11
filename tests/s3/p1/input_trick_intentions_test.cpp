// Port of skate-core/src/input/trick_intentions.rs (#[cfg(test)] mod tests).
#include <cmath>
#include <string_view>

#include "harness.hpp"
#include "s3/core/input/trick_intentions.hpp"

namespace {
using namespace s3::input::trick_intentions;
using s3::rs::to_bits;

DerivedControllerInput controller(float previous_left, float previous_right, float left, float right) {
    std::array<std::uint32_t, 26> words{};
    words[4] = to_bits(previous_left);
    words[5] = to_bits(previous_right);
    words[11] = to_bits(left);
    words[12] = to_bits(right);
    words[9] = to_bits(0.25f);
    words[10] = to_bits(-0.5f);
    return DerivedControllerInput::from_words(words);
}
bool has(const std::vector<TrickIntent>& output, std::string_view name) {
    for (const auto& i : output)
        if (i.name == name) return true;
    return false;
}
const TrickIntent& find(const std::vector<TrickIntent>& output, std::string_view name) {
    for (const auto& i : output)
        if (i.name == name) return i;
    S3_PANIC("called `Option::unwrap()` on a `None` value");
}

S3_TEST(publishes_holds_edges_and_tweaks) {
    const auto output = produce(controller(0.0f, 0.25f, 1.0f, 0.25f));
    CHECK(has(output, "LeftAirGrab"));
    CHECK(!has(output, "NewLeftAirGrab"));
    CHECK(!has(output, "NewRightAirGrab"));
    CHECK_EQ(find(output, "TweakX").value, 0.25f);
    CHECK_EQ(find(output, "TweakY").value, -0.5f);
    const float angle = find(output, "BoardAdjustAngle").value;
    const float magnitude = find(output, "BoardAdjustMag").value;
    CHECK(std::fabs(angle - 0.4636476f) < 0.00001f);
    CHECK(std::fabs(magnitude - 0.559017f) < 0.00001f);
}

S3_TEST(board_adjust_cardinals_follow_fill_register_order) {
    using namespace s3::rs::f32::consts;
    const float cases[4][3] = {
        {0.0f, -1.0f, 0.0f},
        {1.0f, 0.0f, FRAC_PI_2},
        {0.0f, 1.0f, PI},
        {-1.0f, 0.0f, -FRAC_PI_2},
    };
    for (const auto& c : cases) {
        std::array<std::uint32_t, 26> words{};
        words[9] = to_bits(c[0]);
        words[10] = to_bits(c[1]);
        const auto output = produce(DerivedControllerInput::from_words(words));
        const float angle = find(output, "BoardAdjustAngle").value;
        CHECK_MSG(std::fabs(angle - c[2]) < 0.00001f, "x=" << c[0] << ", y=" << c[1] << ", angle=" << angle);
    }
}
}  // namespace
