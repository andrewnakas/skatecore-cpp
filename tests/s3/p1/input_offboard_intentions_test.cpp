// Port of skate-core/src/input/offboard_intentions/tests.rs.
#include <cmath>
#include <limits>
#include <string_view>
#include <vector>

#include "harness.hpp"
#include "s3/core/input/offboard_intentions.hpp"

namespace {
using namespace s3::input::offboard_intentions;
using s3::rs::from_bits;
using s3::rs::to_bits;
using Words = std::array<std::uint32_t, 26>;

std::vector<OffboardIntent> values(Words words, std::uint32_t flags, bool blocked) {
    return produce_discrete(DerivedControllerInput::from_words(words), flags, blocked);
}
bool has(const std::vector<OffboardIntent>& output, std::string_view name) {
    for (const auto& intent : output)
        if (intent.name == name) return true;
    return false;
}
const OffboardIntent& find(const std::vector<OffboardIntent>& output, std::string_view name) {
    for (const auto& intent : output)
        if (intent.name == name) return intent;
    S3_PANIC("called `Option::unwrap()` on a `None` value");
}
Words toggle_words(bool previous, float timer) {
    Words words{};
    words[6] = previous ? (1u << 22) : 0u;
    words[13] = 1u << 22;
    words[23] = to_bits(timer);
    return words;
}

S3_TEST(toggle_rise_and_short_hold_are_distinct_from_continuous_hold) {
    const std::string_view new_ = "NewToggleOffBoardState";
    const std::string_view held = "ToggleOffBoardState";
    CHECK(has(values(toggle_words(false, 9.0f), 0, false), new_));
    CHECK(has(values(toggle_words(true, 0.02f), 0, false), new_));
    const auto boundary = values(toggle_words(true, from_bits(0x3cf5c28fu)), 0, false);
    CHECK(!has(boundary, new_));
    CHECK(has(boundary, held));
    const auto nan_timer = values(toggle_words(true, std::numeric_limits<float>::quiet_NaN()), 0, false);
    CHECK(has(nan_timer, new_));
    Words released_words = toggle_words(true, 0.0f);
    released_words[13] = 0;
    const auto released = values(released_words, 0, false);
    CHECK(!has(released, new_));
    CHECK(!has(released, held));
}

S3_TEST(physical_and_modifier_gates_block_both_but_actor_gate_only_blocks_new) {
    const Words words = toggle_words(false, 0.0f);
    const auto disabled = values(words, 1u << 10, false);
    CHECK(!has(disabled, "NewToggleOffBoardState"));
    CHECK(has(disabled, "ToggleOffBoardState"));
    Words modifier = words;
    modifier[13] |= 1u << 29;
    for (const auto& output : {values(words, 0, true), values(modifier, 0, false)}) {
        CHECK(!has(output, "NewToggleOffBoardState"));
        CHECK(!has(output, "ToggleOffBoardState"));
    }
}

S3_TEST(y_remains_toggle_for_stock_offboard_mount_not_board_recall) {
    //S3 listener8259A604..A6C0 has no physical-state input. The stock
    //OffBoard/OBGround/Default graph consumes both of these for remount.
    const auto output = values(toggle_words(false, 0.0f), 0, false);
    CHECK(has(output, "NewToggleOffBoardState"));
    CHECK(has(output, "ToggleOffBoardState"));
    CHECK(!has(output, "OB_RetrieveBoard"));
}

S3_TEST(jump_requires_new_button_and_does_not_inherit_toggle_gates) {
    Words words{};
    words[13] = (1u << 23) | (1u << 29);
    words[22] = to_bits(0.01f);
    CHECK(has(values(words, UINT32_MAX, true), "OB_Jump"));
    words[6] = 1u << 23;
    CHECK(!has(values(words, 0, false), "OB_Jump"));
}

S3_TEST(full_trigger_edges_publish_board_actions_together_and_do_not_repeat) {
    Words words{};
    words[11] = to_bits(0.99f);
    CHECK(!has(values(words, 0, false), "OB_RetrieveBoard"));
    words[11] = to_bits(1.0f);
    words[12] = to_bits(1.0f);
    const auto output = values(words, UINT32_MAX, true);
    std::vector<std::string_view> board;
    for (const auto& i : output) {
        const std::string_view suffix = "Board";
        if (i.name.size() >= suffix.size() && i.name.substr(i.name.size() - suffix.size()) == suffix)
            board.push_back(i.name);
    }
    CHECK(board == (std::vector<std::string_view>{"OB_DropBoard", "OB_ThrowBoard", "OB_RetrieveBoard"}));
    words[4] = words[11];
    words[5] = words[12];
    CHECK(!has(values(words, 0, false), "OB_RetrieveBoard"));
    words[5] = 0;
    const auto right_only = values(words, 0, false);
    CHECK(!has(right_only, "OB_DropBoard"));
    CHECK(has(right_only, "OB_ThrowBoard"));
    CHECK(has(right_only, "OB_RetrieveBoard"));
}

S3_TEST(sprint_uses_competing_held_timers_and_axes_preserve_zero_presence) {
    Words words{};
    words[20] = to_bits(0.01f);
    auto output = values(words, UINT32_MAX, true);
    CHECK(has(output, "OB_Sprint"));
    CHECK(!has(output, "OB_LookAtX"));
    CHECK(!has(output, "OB_LookAtY"));
    CHECK(has(output, "OB_AirBodyTweakX"));
    CHECK(has(output, "OB_AirBodyTweakY"));
    words[21] = to_bits(0.01f);
    CHECK(!has(values(words, 0, false), "OB_Sprint"));
    words[9] = to_bits(-0.5f);
    words[13] = 1u << 30;
    output = values(words, 0, false);
    CHECK(has(output, "OB_DoAirBodyTweak"));
    CHECK_EQ(find(output, "OB_LookAtX").value, -0.5f);
    CHECK_EQ(find(output, "OB_AirBodyTweakX").value, -0.5f);
}

std::array<OffboardIntent, 4> analog(float x, float z, std::array<float, 4> forward,
                                     std::optional<std::array<float, 4>> correction) {
    Words words{};
    words[7] = to_bits(x);
    words[8] = to_bits(z);
    return produce_analog(DerivedControllerInput::from_words(words), AnalogObservation{forward, correction});
}

S3_TEST(movement_preserves_signed_projection_and_raw_axes) {
    const auto output = analog(0.75f, -0.5f, {0.0f, 8.0f, 3.0f, 0.0f}, std::nullopt);
    CHECK(std::fabs(output[0].value + 0.5f) < 1e-6f);
    CHECK_EQ(output[1].value, -0.5f);
    CHECK_EQ(output[2].value, 0.75f);
    CHECK(std::fabs(output[3].value - std::sqrt(0.75f * 0.75f + 0.25f)) < 1e-6f);
    const auto mirrored = analog(0.75f, -0.5f, {0.0f, 0.0f, -3.0f, 0.0f}, std::nullopt);
    CHECK(std::fabs(mirrored[0].value - 0.5f) < 1e-6f);
}

S3_TEST(obstacle_correction_stops_direct_approach_but_preserves_tangent_motion) {
    const std::array<float, 4> forward = {0.0f, 0.0f, 1.0f, 0.0f};
    const std::optional<std::array<float, 4>> wall = std::array<float, 4>{0.0f, 0.0f, -1.0f, 0.0f};
    CHECK_EQ(analog(0.0f, 1.0f, forward, wall)[0].value, 0.0f);
    const auto tangent = analog(0.6f, 0.8f, {1.0f, 0.0f, 0.0f, 0.0f}, wall);
    CHECK(std::fabs(tangent[0].value - 0.6f) < 1e-6f);
    CHECK(std::fabs(analog(0.0f, -0.5f, forward, wall)[0].value + 0.5f) < 1e-6f);
    //The source intentionally does not normalize the supplied correction vector.
    CHECK(std::fabs(analog(0.0f, 1.0f, forward, std::array<float, 4>{0.0f, 0.0f, -0.5f, 0.0f})[0].value - 0.75f) <
          1e-6f);
}

S3_TEST(zero_and_tiny_headings_use_original_length_threshold) {
    CHECK_EQ(analog(0.0f, 1.0f, {0.0f, 0.0f, 0.0f, 0.0f}, std::nullopt)[0].value, 0.0f);
    CHECK_EQ(analog(0.0f, 1.0f, {0.0f, 0.0f, 0.5e-6f, 0.0f}, std::nullopt)[0].value, 0.0f);
    CHECK(std::fabs(analog(0.0f, 1.0f, {0.0f, 0.0f, 2.0e-6f, 0.0f}, std::nullopt)[0].value - 1.0f) < 1e-6f);
    const auto zero = analog(0.0f, 0.0f, {0.0f, 0.0f, 1.0f, 0.0f}, std::nullopt);
    for (const auto& intent : zero) CHECK(intent.value == 0.0f);
}
}  // namespace
