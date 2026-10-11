// Port of skate-game/src/input/controllers.rs.
#include "s3/game/input/controllers.hpp"

namespace s3::game::input::controllers {

using ::s3::input::gameplay_map::GameplayActions;
using ::s3::input::history::HistoryRecord;
using ::s3::input::history::PadHistory;
using ::s3::input::pad::Pad;

namespace {
std::optional<std::size_t> ready_device(const std::array<ControllerStatus, DEVICE_SLOTS>& status) {
    for (std::size_t i = 0; i < status.size(); ++i)
        if (status[i] == ControllerStatus::ready()) return i;
    return std::nullopt;
}
}  // namespace

std::tuple<bool, bool, bool> ControllerInput::session_marker_actions() const {
    const auto device = ready_device(status);
    if (!device) return {false, false, false};
    const Pad& pad = pads[*device];
    if (pad.count() == 0) return {false, false, false};
    auto flags = [&](std::size_t i) -> std::uint32_t {
        const auto& records = pad.records();
        return i < records.size() ? records[i][1] : 0u;
    };
    const bool modifier = (flags(8) & 0xff00u) != 0;
    return {modifier, modifier && (flags(1) & 0xff000000u) != 0, modifier && (flags(0) & 0xff00u) != 0};
}

RawInput ControllerInput::raw_input() const {
    const auto device = ready_device(status);
    return device ? raw[*device] : RawInput{};
}

void ControllerInput::discard_gameplay() {
    history = PadHistory::new_();
    for (auto& pad : pads) pad = Pad::new_();
    mapped_actions = {};
}

GameplayActions ControllerInput::player_actions() const {
    const std::size_t device = ready_device(status).value_or(0);
    return GameplayActions::from_pad(pads[device]);
}

::s3::input::tick::TickInput ControllerInput::tick_input() const {
    const auto device = ready_device(status);
    const bool controller_available = device.has_value();
    const GameplayActions actions =
        device ? GameplayActions::from_pad(pads[*device]) : GameplayActions::from_values(std::array<float, 18>{});
    return ::s3::input::tick::TickInput::new_(tick, actions, controller_available);
}

void ControllerInput::collect(std::array<DeviceSample, DEVICE_SLOTS> samples) {
    const std::size_t next = active ^ 1u;
    for (std::size_t device = 0; device < DEVICE_SLOTS; ++device) {
        const DeviceSample& sample = samples[device];
        if (sample.is_ok()) {
            const DevicePacket& packet = sample.unwrap();
            RawInput r;
            r.buttons = packet.state.buttons;
            for (std::size_t i = 0; i < 2; ++i) {
                r.triggers[i] = float(packet.state.triggers[i]) / 255.0f;
                r.left[i] = float(packet.state.left[i]) / 32768.0f;
                r.right[i] = float(packet.state.right[i]) / 32768.0f;
            }
            raw[device] = r;
            // 8296D480 sets byte13 only when capability SubType == 7.
            const auto values =
                ::s3::input::xbox::convert(packet.state, static_cast<std::uint8_t>(packet.subtype == 7 ? 1 : 0));
            cache[next][device] = HistoryRecord::new_(values.data(), values.size());
            packet_numbers[device] = packet.number;
            status[device] = ControllerStatus::ready();
        } else {
            raw[device] = RawInput{};
            cache[next][device].clear_count();
            packet_numbers[device] = std::nullopt;
            status[device] = ControllerStatus::unavailable(sample.unwrap_err());
        }
    }
    active = next;
    history.publish(cache[active]);
    publications += 1;
}

bool ControllerInput::publish_actions() {
    tick = tick + 1u;  // wrapping_add
    if (!history.drain_to_latest(pads)) return false;
    for (std::size_t device = 0; device < DEVICE_SLOTS; ++device) {
        GameplayActions actions = GameplayActions::from_pad(pads[device]);
        for (std::uint32_t i = 0; i < 18; ++i) mapped_actions[device][i] = actions.value(64u + i);
    }
    consumed_batches += 1;
    return true;
}

}  // namespace s3::game::input::controllers
