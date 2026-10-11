// Port of skate-core/src/input/wipeout_intentions.rs.
// Wipeout intention helper8259BC28, called by Fill825999F0 at8259AA6C.
#include "s3/core/input/wipeout_intentions.hpp"

#include <string_view>

namespace s3::input::wipeout_intentions {

using rs::from_bits;

std::vector<RidingIntent> produce(const DerivedControllerInput& controller, std::uint32_t actor_flags_1908,
                                  std::uint32_t physical_flags_204) {
    const auto& words = controller.words();
    const std::uint32_t old_buttons = words[6];
    const std::uint32_t buttons = words[13];
    auto axis = [&words](std::size_t slot) { return from_bits(words[slot]); };
    auto rising = [&](std::uint32_t bit) {
        return (old_buttons & (1u << bit)) == 0u && (buttons & (1u << bit)) != 0u;
    };
    auto trigger_edge = [&](std::size_t previous, std::size_t current) {
        return axis(previous) != 1.0f && axis(current) == 1.0f;
    };
    std::vector<RidingIntent> output;
    output.reserve(7);
    auto emit = [&output](std::string_view name, float value) { output.push_back(RidingIntent{name, value}); };

    //8259BC7C..BCEC: both names always exist, including disabled/centered zeros.
    const bool gesture = (physical_flags_204 & 0x80u) != 0;
    emit("WipeoutGestureX", gesture ? axis(9) : 0.0f);
    emit("WipeoutGestureY", gesture ? axis(10) : 0.0f);
    const bool control = (physical_flags_204 & 0x100u) != 0;
    const float x = control ? axis(7) : 0.0f;
    const float y = control ? axis(8) : 0.0f;
    if (x != 0.0f) {
        emit("WipeoutControlX", x);
    }
    if (y != 0.0f) {
        emit("WipeoutControlY", y);
    }
    //8259BD28..BDBC: raw rising edges, not push repeat timers or held buttons.
    if (rising(21) || rising(23)) {
        emit("WipeOutRecover", 1.0f);
    }
    //8259BDCC..BEF0: exact full trigger endpoints, both stick buttons, and
    //a fresh edge from any of the four; actor disablebit6 gates this request.
    if ((actor_flags_1908 & 0x40u) == 0 && axis(11) == 1.0f && axis(12) == 1.0f &&
        (buttons & 0xC0000000u) == 0xC0000000u &&
        (trigger_edge(4, 11) || trigger_edge(5, 12) || rising(31) || rising(30))) {
        emit("WipeOutRequest", 1.0f);
    }
    //8259BEFC..BF3C: only the second trigger's new full endpoint.
    if ((physical_flags_204 & 0x40u) != 0 && trigger_edge(5, 12)) {
        emit("WipeOutPushOff", 1.0f);
    }
    return output;
}

}  // namespace s3::input::wipeout_intentions
