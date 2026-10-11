// Port of skate-game/src/input/controllers.rs.
//! Persistent platform-input owner. Device collection runs on host frames;
//! this does not choose the skater simulation clock or advance Derived input.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <tuple>
#include <variant>

#include "s3/core/input/gameplay_map.hpp"
#include "s3/core/input/history.hpp"
#include "s3/core/input/pad.hpp"
#include "s3/core/input/tick.hpp"
#include "s3/core/input/xbox.hpp"
#include "s3/core/rs.hpp"
#include "s3/game/input/platform.hpp"

namespace s3::game::input::controllers {

using ::s3::input::history::DEVICE_SLOTS;
using platform::DeviceError;
using platform::DevicePacket;

/// `#[derive(Clone, Copy, Debug, PartialEq, Eq)] enum ControllerStatus`.
struct ControllerStatus {
    struct Unpolled {};
    struct Ready {};
    struct Unavailable {
        DeviceError _0;
    };
    std::variant<Unpolled, Ready, Unavailable> value;

    static ControllerStatus unpolled() { return ControllerStatus{Unpolled{}}; }
    static ControllerStatus ready() { return ControllerStatus{Ready{}}; }
    static ControllerStatus unavailable(DeviceError e) { return ControllerStatus{Unavailable{e}}; }
    bool is_ready() const { return std::holds_alternative<Ready>(value); }
};
inline bool operator==(const ControllerStatus& a, const ControllerStatus& b) {
    if (a.value.index() != b.value.index()) return false;
    if (auto* x = std::get_if<ControllerStatus::Unavailable>(&a.value))
        return x->_0 == std::get<ControllerStatus::Unavailable>(b.value)._0;
    return true;
}
inline bool operator!=(const ControllerStatus& a, const ControllerStatus& b) { return !(a == b); }

/// `#[derive(Clone, Copy, Default)]`.
struct RawInput {
    std::uint16_t buttons = 0;
    std::array<float, 2> triggers{};
    std::array<float, 2> left{};
    std::array<float, 2> right{};
};

/// `Result<DevicePacket, DeviceError>` (platform sample).
using DeviceSample = rs::Result<DevicePacket, DeviceError>;

/// `impl Default for ControllerInput` -> member initialisers.
struct ControllerInput {
    // Private in Rust.
    std::array<RawInput, DEVICE_SLOTS> raw{};
    std::array<std::array<::s3::input::history::HistoryRecord, DEVICE_SLOTS>, 2> cache{};
    std::size_t active = 0;
    ::s3::input::history::PadHistory history = ::s3::input::history::PadHistory::new_();
    std::array<::s3::input::pad::Pad, DEVICE_SLOTS> pads{};
    // Public in Rust.
    std::array<ControllerStatus, DEVICE_SLOTS> status = {ControllerStatus::unpolled(), ControllerStatus::unpolled(),
                                                         ControllerStatus::unpolled(), ControllerStatus::unpolled()};
    std::array<std::optional<std::uint32_t>, DEVICE_SLOTS> packet_numbers{};
    std::array<std::array<float, 18>, DEVICE_SLOTS> mapped_actions{};
    std::uint64_t publications = 0;
    std::uint64_t consumed_batches = 0;
    // Private in Rust.
    std::uint64_t tick = 0;

    /// `ControllerInput::default()`.
    static ControllerInput default_() { return ControllerInput{}; }

    /// Original input.cfg: LB.held && DPadD.pressed / LB.held && DPadU.held.
    /// Use the same debounced native Pad publication as ordinary gameplay.
    std::tuple<bool, bool, bool> session_marker_actions() const;
    RawInput raw_input() const;

    // Replay controls must never queue a flick or a button press for gameplay.
    void discard_gameplay();
    ::s3::input::gameplay_map::GameplayActions player_actions() const;

    /// pub(super)
    ::s3::input::tick::TickInput tick_input() const;
    /// TU3 8296D288/8296D0D0: write the inactive cache, publish one four-device
    /// batch. Even an unchanged platform packet is sampled; packet-number
    /// deduplication would alter native Pad edge/repeat behavior.
    void collect(std::array<DeviceSample, DEVICE_SLOTS> samples);

    /// TU3 82699230 drains first, then updates each Pad once. The game owns
    /// this snapshot for subsequent consumers; Derived timers require the
    /// actual actor timestep and state flags and are not driven by render dt.
    bool publish_actions();

    /// Test-only (`input/tests/controllers.rs`): feed test packets through the
    /// production conversion, cache, history and Pad. Defined in
    /// tests/s3/game/input_tests_controllers_test.cpp.
    void sample_raw_for_test(::s3::input::xbox::XboxState state);
};

}  // namespace s3::game::input::controllers
