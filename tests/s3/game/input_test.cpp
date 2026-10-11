// Port of the skate-game input tests:
//   input/tests/controllers.rs (mounted as input::controllers::tests), input/gesture_mapping.rs tests,
//   input/platform.rs cache_tests, input/gesture_input.rs tests (private-asset test; runs on the pack).
#include <array>
#include <stdexcept>

#include "game_test_assets.hpp"
#include "harness.hpp"
#include "s3/core/input/xbox.hpp"
#include "s3/game/input.hpp"
#include "s3/game/input/controllers.hpp"
#include "s3/game/input/gesture_input.hpp"
#include "s3/game/input/gesture_mapping.hpp"
#include "s3/game/input/platform.hpp"

using s3::graph::intents::IntentMap;
using s3::input::xbox::XboxState;
using namespace s3::game::input;
using controllers::ControllerInput;
using controllers::ControllerStatus;
using controllers::DeviceSample;
using platform::DeviceError;
using platform::DevicePacket;

// ---- input/tests/controllers.rs --------------------------------------------------------------

namespace s3::game::input::controllers {
/// Feed test packets through the production conversion, cache, history and Pad.
/// Only the operating-system read is supplied by the test.
void ControllerInput::sample_raw_for_test(XboxState state) {
    collect({DeviceSample(DevicePacket{static_cast<std::uint32_t>(publications) + 1u, state, 1}),
             DeviceSample(rs::make_err(DeviceError::disconnected())),
             DeviceSample(rs::make_err(DeviceError::disconnected())),
             DeviceSample(rs::make_err(DeviceError::disconnected()))});
    if (!publish_actions()) throw std::runtime_error("assertion failed: self.publish_actions()");
}
}  // namespace s3::game::input::controllers

namespace {

DeviceSample packet(std::uint32_t number, std::uint16_t buttons, std::array<std::int16_t, 2> left) {
    XboxState state{};
    state.buttons = buttons;
    state.left = left;
    return DeviceSample(DevicePacket{number, state, 1});
}
std::array<DeviceSample, 4> all(const DeviceSample& s) { return {s, s, s, s}; }
DeviceSample disconnected() { return DeviceSample(s3::rs::make_err(DeviceError::disconnected())); }

}  // namespace

S3_TEST(raw_packets_reach_stock_actions_without_losing_device_identity) {
    ControllerInput input{};
    input.collect({
        packet(1, 0x1000, {32767, 0}),
        packet(2, 0x2000, {-32768, 0}),
        packet(3, 0x4000, {0, 0}),
        packet(4, 0x8000, {0, 0}),
    });
    CHECK(input.publish_actions());
    CHECK(std::fabs(input.mapped_actions[0][0] - 1.0f) < 0.000001f);
    CHECK(std::fabs(input.mapped_actions[1][0] + 1.0f) < 0.000001f);
    CHECK_EQ(input.mapped_actions[0][16], 1.0f);  // GP_AFace
    CHECK_EQ(input.mapped_actions[1][17], 1.0f);  // GP_BFace
    CHECK_EQ(input.mapped_actions[2][14], 1.0f);  // GP_XFace
    CHECK_EQ(input.mapped_actions[3][15], 1.0f);  // GP_YFace
    const std::array<std::optional<std::uint32_t>, 4> numbers{1u, 2u, 3u, 4u};
    CHECK(input.packet_numbers == numbers);
}

S3_TEST(unchanged_packet_numbers_do_not_suppress_publication_and_empty_drain_does_not_advance) {
    ControllerInput input{};
    for (int i = 0; i < 4; ++i) {
        input.collect(all(packet(7, 0x1000, {0, 0})));
        CHECK(input.publish_actions());
    }
    CHECK_EQ(input.publications, 4u);
    CHECK_EQ(input.consumed_batches, 4u);
    CHECK((input.pads[0].records()[12][1] & 0x01000000u) != 0u);
    const auto prior = input.pads;
    CHECK(!input.publish_actions());
    CHECK(input.pads == prior);
}

S3_TEST(disconnect_publishes_zero_count_and_reconnect_reinitializes_pad_records) {
    ControllerInput input{};
    input.collect(all(packet(1, 0x1000, {32767, 0})));
    input.publish_actions();
    input.collect(all(disconnected()));
    input.publish_actions();
    const std::array<std::array<float, 18>, 4> zero{};
    CHECK(input.mapped_actions == zero);
    for (const auto& pad : input.pads) CHECK_EQ(pad.count(), 0u);
    input.collect(all(packet(1, 0, {0, 0})));
    input.publish_actions();
    CHECK(input.mapped_actions == zero);
    const std::array<std::uint32_t, 4> record{0, 0, 0, 1};
    CHECK(input.pads[0].records()[12] == record);
}

S3_TEST(alternate_guitar_subtype_uses_verified_conversion_mask_and_errors_stay_visible) {
    ControllerInput input{};
    XboxState state{};
    state.buttons = 0;
    state.triggers = {255, 255};
    state.left = {0, 0};
    state.right = {-32768, 32767};
    const DevicePacket special{1, state, 7};
    input.collect({
        DeviceSample(special),
        DeviceSample(s3::rs::make_err(DeviceError::capabilities(5))),
        disconnected(),
        disconnected(),
    });
    input.publish_actions();
    CHECK_EQ(input.mapped_actions[0][6], 0.0f);
    CHECK_EQ(input.mapped_actions[0][7], 0.0f);
    CHECK_EQ(input.mapped_actions[0][3], 0.0f);
    CHECK_EQ(input.mapped_actions[0][4], 0.0f);
    CHECK(input.status[1] == ControllerStatus::unavailable(DeviceError::capabilities(5)));
    const std::array<float, 18> zero{};
    CHECK(input.mapped_actions[1] == zero);
}

S3_TEST(tick_input_is_the_single_fixed_tick_snapshot) {
    ControllerInput input{};
    input.collect(all(packet(1, 0, {19661, 0})));
    CHECK(input.publish_actions());

    const auto first = input.tick_input();
    CHECK_EQ(first.tick(), 1u);
    CHECK(first.controller_available());
    auto first_actions = first.actions();
    const float first_turn = first_actions.value(64);

    // A second consumer reads the same published tick without touching the
    // platform cache or remapping the controller.
    const auto second = input.tick_input();
    auto second_actions = second.actions();
    CHECK_EQ(second.tick(), first.tick());
    CHECK_EQ(second_actions.value(64), first_turn);
}

S3_TEST(unavailable_controllers_publish_zero_gameplay_actions) {
    ControllerInput input{};
    input.collect(all(packet(1, 0x1000, {32767, 0})));
    CHECK(input.publish_actions());
    CHECK(input.tick_input().controller_available());

    input.collect(all(disconnected()));
    CHECK(input.publish_actions());
    const auto snapshot = input.tick_input();
    CHECK(!snapshot.controller_available());
    const std::array<float, 18> zero{};
    CHECK(snapshot.actions().values() == zero);
}

// ---- input/gesture_mapping.rs tests ----------------------------------------------------------

S3_TEST(tre_flip_uses_original_mirror_mapping_and_presence_not_strength) {
    using gesture_mapping::select;
    using gesture_catalog::Group;
    IntentMap ag{};
    ag.insert("360Flip", 0.0f);
    CHECK_EQ(std::string(select(Group::Square, ag, false)), std::string("360Flip"));
    CHECK_EQ(std::string(select(Group::Square, ag, true)), std::string("Laserflip"));
    ag.insert("DontMirrorTrick", 0.0f);
    CHECK_EQ(std::string(select(Group::Square, ag, true)), std::string("360Flip"));
}

S3_TEST(first_update_retains_launch_intent_then_successor_is_cleaned_on_exit) {
    using gesture_catalog::Group;
    IntentMap ag{};
    ag.insert("Kickflip", 1.0f);
    IntentMap mg{};
    gesture_mapping::State state{};
    state.begin(Group::Square, std::nullopt, ag, mg, false);
    state.update(mg);
    CHECK(mg.contains_key("Kickflip"));
    state.update(mg);
    CHECK(!mg.contains_key("Kickflip"));
    CHECK(mg.contains_key("KickflipHold"));
    state.end(mg);
    CHECK(mg.is_empty());
}

// ---- input/platform.rs cache_tests -----------------------------------------------------------

S3_TEST(capability_cache_refreshes_and_never_caches_errors) {
    using R = s3::rs::Result<std::uint8_t, DeviceError>;
    const auto start = std::chrono::steady_clock::now();
    platform::CapabilityCache cache{};
    CHECK(cache.get(start, [] { return R(std::uint8_t{1}); }) == R(std::uint8_t{1}));
    CHECK(cache.get(start + std::chrono::milliseconds(999),
                    []() -> R { throw std::runtime_error("redundant capability query"); }) == R(std::uint8_t{1}));
    CHECK(cache.get(start + std::chrono::seconds(1), [] { return R(std::uint8_t{2}); }) == R(std::uint8_t{2}));
    cache.invalidate();
    CHECK(cache.get(start, [] { return R(s3::rs::make_err(DeviceError::capabilities(5))); }) ==
          R(s3::rs::make_err(DeviceError::capabilities(5))));
    CHECK(cache.get(start, [] { return R(std::uint8_t{3}); }) == R(std::uint8_t{3}));
    cache.invalidate();
    CHECK(cache.get(start, [] { return R(std::uint8_t{4}); }) == R(std::uint8_t{4}));
}

// ---- input/gesture_input.rs tests (#[ignore] private stock joystick + collection data) --------

S3_TEST(stock_360_gesture_reaches_native_square_mapping) {
    const auto* vfs = s3test::game::assets();
    if (vfs == nullptr) return;
    auto loaded = gesture_input::GestureInput::load(*vfs, s3test::game::ASSET_ROOT);
    CHECK(loaded.is_ok());
    auto input = std::move(loaded.unwrap());
    IntentMap ag{};
    std::vector<std::array<float, 2>> points;
    for (const auto& p : input.recognizers[0].second.patterns())
        if (p.name == "360Flip") points = p.points;
    CHECK(!points.empty());
    input.publish({std::array<float, 2>{0.0f, 0.0f}, std::array<float, 2>{0.0f, 0.0f}}, 0, 0, 100, ag);
    for (int i = 0; i < 30; ++i) {
        ag.clear();
        input.publish({std::array<float, 2>{0.0f, 0.0f}, std::array<float, 2>{points[0][0], -points[0][1]}}, 0,
                      0, 100, ag);
        CHECK(!ag.contains_key("Trick"));
    }
    for (std::size_t i = 1; i < points.size(); ++i) {
        ag.clear();
        input.publish({std::array<float, 2>{0.0f, 0.0f}, std::array<float, 2>{points[i][0], -points[i][1]}}, 0,
                      0, 100, ag);
    }
    CHECK(ag.contains_key("360Flip"));
    const char* selected = gesture_mapping::select(gesture_catalog::Group::Square, ag, false);
    CHECK(selected != nullptr);
    CHECK_EQ(std::string(selected), std::string("360Flip"));
    CHECK(ag.get("GestureSpeed") != nullptr);
    CHECK_EQ(*ag.get("GestureSpeed"), 1.0f);
}
