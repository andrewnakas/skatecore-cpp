// Port of skate-game/src/physics/controls/offboard_tests.rs.
//! Numerical and adapter regressions, not original-executable parity tests.
#include "harness.hpp"
#include "s3/game/physics/controls.hpp"

using namespace s3::game::physics::controls;
namespace rs = s3::rs;

namespace {
void close(std::array<float, 2> actual, std::array<float, 2> expected) {
    for (std::size_t i = 0; i < 2; ++i) {
        CHECK_MSG(rs::abs(actual[i] - expected[i]) < 2.0e-6f, "actual/expected mismatch");
    }
}
using M = std::array<std::array<float, 3>, 3>;

struct Packet final : ActionMap {
    std::array<float, 82> values{};
    std::array<std::uint8_t, 82> states{};
    static Packet distinct() {
        Packet p;
        for (std::size_t i = 0; i < 82; ++i) {
            p.values[i] = float(i) / 100.0f;
            p.states[i] = std::uint8_t(i % 2);
        }
        return p;
    }
    float value(std::uint32_t action) override { return values.at(action); }
    std::uint8_t state(std::uint32_t action) override { return states.at(action); }
};
}  // namespace

S3_TEST(offboard_remap_preserves_native_left_axis_sign_and_yaw) {
    const M identity = {{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
    close(camera_relative_axes({0.25f, 0.75f}, identity), {-0.25f, 0.75f});
    const M quarter_turn = {{{0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}}};
    close(camera_relative_axes({0.25f, 0.75f}, quarter_turn), {0.75f, 0.25f});
    close(camera_relative_axes({-0.25f, -0.75f}, quarter_turn), {-0.75f, -0.25f});
    close(camera_relative_axes({0.0f, 0.0f}, quarter_turn), {0.0f, 0.0f});
}

S3_TEST(offboard_remap_flattens_pitch_below_native_threshold) {
    const M pitched = {{{1.0f, 0.0f, 0.0f}, {0.0f, 0.8f, -0.6f}, {0.0f, 0.6f, 0.8f}}};
    close(camera_relative_axes({0.25f, 0.75f}, pitched), {-0.25f, 0.75f});
}

S3_TEST(offboard_remap_retains_camera_basis_at_both_threshold_poles) {
    const float threshold = rs::from_bits(0x3f7d70a4u);
    const float horizontal = rs::sqrt(1.0f - threshold * threshold);
    for (float vertical : {threshold, -threshold}) {
        const M pitched = {{{1.0f, 0.0f, 0.0f}, {0.0f, horizontal, -vertical}, {0.0f, vertical, horizontal}}};
        close(camera_relative_axes({0.25f, 0.75f}, pitched), {-0.25f, 0.75f * horizontal});
    }
    // An exactly vertical camera does not invent a horizontal forward direction.
    const M vertical = {{{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}}};
    close(camera_relative_axes({0.0f, 1.0f}, vertical), {0.0f, 0.0f});
}

S3_TEST(offboard_simulation_adapter_overrides_only_the_sampled_pair) {
    auto packet = Packet::distinct();
    const auto values = packet.values;
    const auto states = packet.states;
    PlayerControls controls{};
    controls.offboard_axes = std::array<float, 2>{-0.25f, 0.0f};
    auto mapped = controls.simulation_actions(packet);
    // The adapter owns the sampled pair, not a borrow of mutable controller state.
    controls.offboard_axes = std::array<float, 2>{0.9f, 0.8f};
    for (std::uint32_t action = 0; action < 82; ++action) {
        float value = values[action];
        std::uint8_t state = states[action];
        if (action == 64) {
            value = -0.25f;
            state = 1;
        } else if (action == 65) {
            value = 0.0f;
            state = 0;
        }
        CHECK_EQ(rs::to_bits(mapped.value(action)), rs::to_bits(value));
        CHECK_EQ(mapped.state(action), state);
    }
}

S3_TEST(offboard_inactive_adapter_preserves_every_onboard_action) {
    auto packet = Packet::distinct();
    const auto values = packet.values;
    const auto states = packet.states;
    const PlayerControls controls{};
    auto mapped = controls.simulation_actions(packet);
    for (std::uint32_t action = 0; action < 82; ++action) {
        CHECK_EQ(rs::to_bits(mapped.value(action)), rs::to_bits(values[action]));
        CHECK_EQ(mapped.state(action), states[action]);
    }
}

S3_TEST(offboard_sample_reaches_derived_history_and_raw_tick_clears_override) {
    auto packet = Packet::distinct();
    PlayerControls controls{};
    {
        SimulationActions actions(packet, std::array<float, 2>{-0.25f, 0.75f});
        controls.update(actions, 1.0f / 60.0f, 0.1f, 0);
    }
    CHECK_EQ(controls.controller.words()[7], rs::to_bits(-0.25f));
    CHECK_EQ(controls.controller.words()[8], rs::to_bits(0.75f));
    controls.offboard_axes = std::array<float, 2>{-0.25f, 0.75f};
    controls.update(packet, 1.0f / 60.0f, 0.1f, 0);
    CHECK(!controls.offboard_axes.has_value());
    CHECK_EQ(controls.controller.words()[0], rs::to_bits(-0.25f));
    CHECK_EQ(controls.controller.words()[1], rs::to_bits(0.75f));
    CHECK_EQ(controls.controller.words()[7], rs::to_bits(packet.values[64]));
    CHECK_EQ(controls.controller.words()[8], rs::to_bits(packet.values[65]));
}
