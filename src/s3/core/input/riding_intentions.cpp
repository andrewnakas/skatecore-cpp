// Port of skate-core/src/input/riding_intentions.rs.
// Push, brake, body-spin, steering, kick-turn, crouch, powerslide and world-grab intents
// from the TU3 ActionGraph input listener (`825999F0`).
#include "s3/core/input/riding_intentions.hpp"

#include "s3/core/input/angle.hpp"
#include "s3/core/input/steering_intentions.hpp"

namespace s3::input::riding_intentions {

using controller::magnitude;
using rs::from_bits;

std::vector<RidingIntent> produce(const DerivedControllerInput& controller, std::uint32_t actor_flags,
                                  PushPreferences preferences) {
    const auto& words = controller.words();
    const std::uint32_t previous = words[6];
    const std::uint32_t current = words[13];
    const std::array<float, 2> left = {from_bits(words[7]), from_bits(words[8])};
    const bool allowed = (actor_flags & (1u << 11)) == 0;
    auto held = [&](std::uint32_t bit) { return (current & (1u << bit)) != 0 && allowed; };
    auto new_ = [&](std::uint32_t bit, std::size_t timer) {
        return held(bit) && ((previous & (1u << bit)) == 0 || from_bits(words[timer]) < 0.3f);
    };
    bool right = held(21);
    bool left_push = held(23);
    bool new_right = new_(21, 20);
    bool new_left = new_(23, 22);
    const float angle = angle::left_stick_angle(left[0], left[1]);
    const float from_forward = angle >= 0.0f ? from_bits(0x40490fdbu) - angle : -from_bits(0x40490fdbu) - angle;
    const float length = magnitude(rs::mul_add(left[1], left[1], left[0] * left[0]));
    if (preferences.automatic_push_enabled && allowed && !(right || left_push || new_right || new_left) &&
        rs::abs(from_forward) < 0.6f && length > 0.8f) {
        if (preferences.automatic_push_right) {
            right = true;
            new_right = true;
        } else {
            left_push = true;
            new_left = true;
        }
    }
    if ((actor_flags & (1u << 6)) != 0 && (right || new_right) && (left_push || new_left)) {
        right = false;
        left_push = false;
        new_right = false;
        new_left = false;
    }
    std::vector<RidingIntent> output;
    output.reserve(17);
    auto emit = [&output](std::string_view name, float value) { output.push_back(RidingIntent{name, value}); };
    if ((current & (1u << 20)) != 0 && (actor_flags & (1u << 7)) == 0) {
        emit("Brake", 1.0f);
    }
    if (new_right || new_left) {
        emit("NewPush", 1.0f);
    }
    if (right) {
        emit("RightPush", 1.0f);
        emit("Pushing", 1.0f);
    }
    if (left_push) {
        emit("LeftPush", 1.0f);
        emit("Pushing", 1.0f);
    }
    //8259AC1C..AC54 emits this ordered pair from current left X. Actor1908
    //bit1 gates both; steering's bit0 and the physical state do not gate it.
    //BodySpin's later animation-attribute handler owns its sign conversion.
    if (left[0] != 0.0f && (actor_flags & (1u << 1)) == 0) {
        emit("BodySpin", left[0]);
        emit("PhysBodySpin", left[0]);
    }
    //8259ACDC..ACE8 selects KickTurn from current left X, not shaped Turn.
    if (left[0] != 0.0f && (actor_flags & 1u) == 0) {
        emit("KickTurn", left[0]);
    }
    const auto steering = steering_intentions::produce(left, actor_flags);
    if (steering.hard_turn_crouch) {
        emit("HardTurnCrouch", *steering.hard_turn_crouch);
    }
    if (steering.hard_turn) {
        emit("HardTurn", *steering.hard_turn);
    }
    //8259A398..A424, emission key8259AE30..AE3C: the stick-based crouch ramp is
    //multiplied by literal zero (f24). The two fsel instructions select the
    //MAXIMUM of that result and the two trigger values, not a subtraction.
    //An active gate can therefore intentionally emit Crouch with value zero.
    const float absolute = rs::abs(angle);
    const float start = from_bits(0x3ff5c28fu);  // 1.92
    const float end = from_bits(0x40166666u);    // 2.35
    float ramp;
    if (absolute <= start) {
        ramp = 0.0f;
    } else if (absolute < end) {
        ramp = ((absolute - start) * from_bits(0x4014d655u)) * length;
    } else {
        ramp = length;
    }
    const float stick_crouch = ramp * 0.0f;
    const float left_trigger = from_bits(words[11]);
    const float right_trigger = from_bits(words[12]);
    if (absolute > start || left_trigger > 0.0f || right_trigger != 0.0f) {
        const float trigger = left_trigger - right_trigger >= -0.0f ? left_trigger : right_trigger;
        emit("Crouch", stick_crouch - trigger >= -0.0f ? stick_crouch : trigger);
    }
    if (steering.turn) {
        emit("Turn", *steering.turn);
    }

    //8259A430..A50C: start queries and continuous values have distinct open
    //heading windows. Both continuous descriptors can be present together.
    //8259AECC..AF5C gates only actor bit8, independently of push inhibition.
    if ((actor_flags & (1u << 8)) == 0 && length > 0.89999998f) {
        const float SLIDE_SCALE = 0.28004956f;
        const float HALF_PI = from_bits(0x3fc90fdbu);
        if (angle > 0.0f && angle < 0.91000003f) {
            emit("RightSlideStart", 1.0f);
        }
        if (angle > -0.91000003f && angle < 0.0f) {
            emit("LeftSlideStart", 1.0f);
        }
        if (angle > -2.0f && angle < HALF_PI) {
            emit("LeftSlide", -((angle + 2.0f) * SLIDE_SCALE));
        }
        if (angle > -HALF_PI && angle < 2.0f) {
            emit("RightSlide", (2.0f - angle) * SLIDE_SCALE);
        }
    }
    //8259A54C/554 reads current raw flags bit28;8259AF68..7C emits
    //GrabWorld (830BE780, named by initializer82F84EB0) while held.
    //No actor inhibition or rising-edge gate applies to this intention.
    if ((current & (1u << 28)) != 0) {
        emit("GrabWorld", 1.0f);
    }
    return output;
}

}  // namespace s3::input::riding_intentions
