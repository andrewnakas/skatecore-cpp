// Port of skate-core/src/input/riding_intentions_tests.rs.
#include <algorithm>
#include <string_view>
#include <vector>

#include "harness.hpp"
#include "s3/core/input/riding_intentions.hpp"

namespace {
using namespace s3::input::riding_intentions;
using s3::rs::to_bits;
using Words = std::array<std::uint32_t, 26>;

const RidingIntent& find(const std::vector<RidingIntent>& output, std::string_view name) {
    for (const auto& intent : output)
        if (intent.name == name) return intent;
    S3_PANIC("called `Option::unwrap()` on a `None` value");
}
bool any(const std::vector<RidingIntent>& v, bool (*pred)(const RidingIntent&)) {
    return std::any_of(v.begin(), v.end(), pred);
}
bool ends_with(std::string_view s, std::string_view suffix) {
    return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
}
bool contains(std::string_view s, std::string_view needle) { return s.find(needle) != std::string_view::npos; }

DerivedControllerInput snapshot(std::uint32_t previous, std::uint32_t current, float held) {
    Words words{};
    words[6] = previous;
    words[13] = current;
    words[20] = to_bits(held);
    words[22] = to_bits(held);
    return DerivedControllerInput::from_words(words);
}

S3_TEST(right_bumper_publishes_held_grab_world_without_actor_gate) {
    struct Case {
        std::uint32_t previous, current;
        bool expected;
    };
    for (const Case& c : {Case{0, 1u << 28, true}, Case{1u << 28, 1u << 28, true}, Case{1u << 28, 0, false},
                          Case{0, 1u << 29, false}}) {
        for (std::uint32_t actor_flags : {0u, UINT32_MAX}) {
            const auto output = produce(snapshot(c.previous, c.current, 0.5f), actor_flags, PushPreferences{});
            std::vector<RidingIntent> values;
            for (const auto& i : output)
                if (i.name == "GrabWorld") values.push_back(i);
            CHECK_EQ(values.size(), std::size_t(c.expected ? 1 : 0));
            if (c.expected) {
                CHECK_EQ(values[0].value, 1.0f);
            }
        }
    }
}

S3_TEST(crouch_preserves_zero_presence_and_uses_maximum_trigger) {
    Words words{};
    words[8] = to_bits(1.0f);
    auto values = produce(DerivedControllerInput::from_words(words), 1, PushPreferences{});
    CHECK_EQ(find(values, "Crouch").value, 0.0f);
    words[11] = to_bits(0.4f);
    words[12] = to_bits(0.7f);
    words[7] = to_bits(0.5f);
    values = produce(DerivedControllerInput::from_words(words), 0, PushPreferences{});
    CHECK_EQ(find(values, "Crouch").value, 0.7f);
    CHECK_EQ(find(values, "KickTurn").value, 0.5f);
    CHECK_NE(find(values, "Turn").value, 0.5f);
}

S3_TEST(held_push_continues_after_new_push_allowance_expires) {
    const std::uint32_t button = 1u << 21;
    const auto first = produce(snapshot(0, button, 0.5f), 0, PushPreferences{});
    CHECK(any(first, [](const RidingIntent& i) { return i.name == "NewPush"; }));
    const auto held = produce(snapshot(button, button, 0.5f), 0, PushPreferences{});
    std::vector<std::string_view> names;
    for (const auto& i : held) names.push_back(i.name);
    CHECK(names == (std::vector<std::string_view>{"RightPush", "Pushing"}));
    CHECK(produce(snapshot(0, button, 0.0f), 1u << 11, PushPreferences{}).empty());
}

S3_TEST(dual_push_gate_requires_both_feet_and_brake_uses_actor_mode) {
    CHECK(!produce(snapshot(0, 1u << 21, 0.0f), 1u << 6, PushPreferences{}).empty());
    CHECK(produce(snapshot(0, (1u << 21) | (1u << 23), 0.0f), 1u << 6, PushPreferences{}).empty());
    const auto brake = snapshot(0, 1u << 20, 0.0f);
    CHECK(produce(brake, 0, PushPreferences{})[0].name == "Brake");
    CHECK(produce(brake, 1u << 7, PushPreferences{}).empty());
}

S3_TEST(powerslide_queries_are_separate_from_signed_continuous_values) {
    auto intents = [](float x, float y, std::uint32_t flags) {
        Words words{};
        words[7] = to_bits(x);
        words[8] = to_bits(y);
        return produce(DerivedControllerInput::from_words(words), flags, PushPreferences{});
    };
    //8259A430..A50C: lateral steering is outside both start windows.
    for (float x : {-1.0f, 1.0f}) {
        const auto values = intents(x, 0.0f, 0);
        CHECK(!any(values, [](const RidingIntent& i) { return ends_with(i.name, "SlideStart"); }));
        CHECK(any(values, [](const RidingIntent& i) { return i.name == "Turn"; }));
    }
    //Diagonal rearward queries have independent, overlapping continuous values.
    const std::pair<float, std::string_view> cases[2] = {{0.6f, "RightSlideStart"}, {-0.6f, "LeftSlideStart"}};
    for (const auto& [x, start] : cases) {
        const auto values = intents(x, -0.8f, 0);
        bool found = false;
        for (const auto& i : values)
            if (i.name == start && i.value == 1.0f) found = true;
        CHECK(found);
        CHECK(any(values, [](const RidingIntent& i) { return i.name == "LeftSlide" && i.value < 0.0f; }));
        CHECK(any(values, [](const RidingIntent& i) { return i.name == "RightSlide" && i.value > 0.0f; }));
        const auto inhibited_push = intents(x, -0.8f, 1u << 11);
        for (const auto& value : values) {
            if (!contains(value.name, "Slide")) continue;
            CHECK(std::find(inhibited_push.begin(), inhibited_push.end(), value) != inhibited_push.end());
        }
        CHECK(!any(intents(x, -0.8f, 1u << 8), [](const RidingIntent& i) { return contains(i.name, "Slide"); }));
    }
    const std::pair<float, float> neutral[4] = {{0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, -1.0f}, {0.3f, -0.4f}};
    for (const auto& [x, y] : neutral) {
        const auto values = intents(x, y, 0);
        CHECK(!any(values, [](const RidingIntent& i) { return ends_with(i.name, "SlideStart"); }));
    }
    CHECK(!any(intents(0.3f, -0.4f, 0), [](const RidingIntent& i) { return contains(i.name, "Slide"); }));
}
}  // namespace
