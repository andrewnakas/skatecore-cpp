// Port of skate-core/src/input/trick_intentions.rs.
// Player grab and aerial trick-control intents.
#include "s3/core/input/trick_intentions.hpp"

namespace s3::input::trick_intentions {

using rs::from_bits;

std::vector<TrickIntent> produce(const DerivedControllerInput& controller) {
    const auto& words = controller.words();
    const float left_trigger = from_bits(words[11]);
    const float right_trigger = from_bits(words[12]);
    const float right_x = from_bits(words[9]);
    const float right_y = from_bits(words[10]);

    // Fill825992D8 advances the grab timers only when the trigger action is
    // exactly 1.0; partial analog values are not a held grab in Skate 3.
    const bool left_ground = left_trigger == 1.0f;
    const bool right_ground = right_trigger == 1.0f;
    const bool left_air = left_trigger > 0.0f;
    const bool right_air = right_trigger > 0.0f;

    std::vector<TrickIntent> output;
    output.reserve(12);
    auto emit = [&output](std::string_view name, float value) { output.push_back(TrickIntent{name, value}); };
    if (left_ground) {
        emit("LeftGroundGrab", 1.0f);
    }
    if (left_air) {
        emit("LeftAirGrab", 1.0f);
    }
    if (right_ground) {
        emit("RightGroundGrab", 1.0f);
    }
    if (right_air) {
        emit("RightAirGrab", 1.0f);
    }

    const std::uint32_t previous_flags = words[6];
    const std::uint32_t current_flags = words[13];
    // TU3 Fill8259A9E4..AA54: B publishes Dismount while held and
    // NewDismount only on its rising edge. No grab/state/actor gate here;
    // the authored graphs decide whether this means dismount or no-foot air.
    // Names: 82F84B20 -> 830BE378, 82F84B08 -> 830C07AC.
    const std::uint32_t dismount_button = 1u << 20;
    if ((current_flags & dismount_button) != 0) {
        emit("Dismount", 1.0f);
        if ((previous_flags & dismount_button) == 0) {
            emit("NewDismount", 1.0f);
        }
    }
    const std::uint32_t dark_flags = (1u << 20) | (1u << 28);
    if ((current_flags & dark_flags) != 0) {
        emit("DarkCatch", 1.0f);
    }
    if ((current_flags & dark_flags) != 0 && (previous_flags & dark_flags) == 0) {
        emit("NewDarkCatch", 1.0f);
    }
    // Fill825999F0 publishes the board-adjust pair from the same normalized
    // stick vector used by the authored air board-adjust states.  The native
    // 82599B08..BB4: v125 = -RStickY is the denominator and v126 =
    // RStickX is the numerator. The quadrant correction also tests -RStickY.
    // This is atan2(RStickX, -RStickY), in that argument order.
    const float board_adjust_magnitude = rs::sqrt(rs::mul_add(right_x, right_x, right_y * right_y));
    if (board_adjust_magnitude > 0.0f) {
        emit("BoardAdjustAngle", rs::atan2(right_x, -right_y));
        emit("BoardAdjustMag", board_adjust_magnitude);
    }
    if (right_x != 0.0f) {
        emit("TweakX", right_x);
    }
    if (right_y != 0.0f) {
        emit("TweakY", right_y);
    }
    //Fill8259A554/8259AF60: current packed bit28 (RB), without an actor gate.
    //The stock ground graph attaches GrabWorld; physics selects valid coping.
    if ((current_flags & (1u << 28)) != 0) {
        emit("GrabWorld", 1.0f);
    }
    //Fill calls8259BB18 with the CURRENT RawControllerInput (words7..13).
    //Descriptor constructors82F84F28..82F84F98 establish these five names.
    if (right_x != 0.0f) {
        emit("HandPlantTweakX", right_x);
    }
    if (right_y != 0.0f) {
        emit("HandPlantTweakY", right_y);
    }
    const std::pair<std::uint32_t, std::string_view> hand[3] = {
        {20, "HandPlantDismount"},
        {21, "HandPlantOneFootRight"},
        {23, "HandPlantOneFootLeft"},
    };
    for (const auto& [bit, name] : hand) {
        if ((current_flags & (1u << bit)) != 0) {
            emit(name, 1.0f);
        }
    }
    return output;
}

}  // namespace s3::input::trick_intentions
