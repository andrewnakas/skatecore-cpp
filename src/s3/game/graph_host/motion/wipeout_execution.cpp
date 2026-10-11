// Port of skate-game/src/graph_host/motion_wipeout_execute.rs (module `motion::wipeout_execution`).
#include "s3/game/graph_host/motion/wipeout_execution.hpp"

#include "s3/core/animation/playback_parameters.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/game/graph_host/motion/execution.hpp"

namespace s3::game::graph_host::motion {

namespace {

float bounded(float value, float limit) {
    //Original fsel comparisons, preserving unordered and signed-zero behavior.
    const float v = -limit - value >= 0.0f ? -limit : value;
    return limit - v >= 0.0f ? v : limit;
}

float positive_turn(float value) { return value >= 0.0f ? value : value + rs::from_bits(0x40C90FDB); }

void set(MotionAnimation& animation, const char* name, float value) {
    animation.set_attribute(s3::animation::playback_parameters::SettableAttribute{
        s3::animation::skeleton_input::name::encode(name), value, false, -1});
}

}  // namespace

rs::Result<rs::Unit, std::string> MotionHost::execute_wipeout(BehaviorId behavior, motion_wipeout::Operation operation,
                                                              std::uint8_t phase) {
    using s3::animation::skeleton_input::name::encode;
    if (operation == motion_wipeout::Operation::EnableGestures) {
        //82BC07A8/07F8, Update82B61BB8 is empty.
        if (phase != 1) wipeout_controls.gestures_enabled = phase == 0;
        return rs::Unit{};
    }
    if (phase > 1) return rs::Unit{};  //Original End82B61BB8.
    if (behavior >= instances.size()) return rs::make_err(std::string("Unallocated Wipeout behavior"));
    auto* instance = instances[behavior].get<Instance::Wipeout>();
    if (instance == nullptr) return rs::make_err(std::string("Wipeout operation/instance mismatch"));
    motion_wipeout::State& state = instance->_0;
    S3_MHOST_SOME(const motion_wipeout::Physical physical_, wipeout_physical,
                   "Wipeout requires completed physical outputs");
    if (phase == 0) {
        S3_MHOST_SOME(const std::uint32_t animation_flags, animation.skater_animation_flags,
                       "Wipeout requires animation stance");
        const bool mirrored = (animation_flags & 0x40000000u) != 0;
        state.ticks = 0;
        state.lean = physical_.hips_right_angle_496;
        const float twist = mirrored ? -physical_.hips_up_angle_500 : physical_.hips_up_angle_500;
        state.twist = positive_turn(twist);
        state.twist_velocity = 0.0f;
        state.released = wipeout_controls.seed_from_air_tweak;
        if (state.released) {
            wipeout_controls.seed_from_air_tweak = false;
            state.gesture = {action_controls.wipeout.air_body_tweak[0], action_controls.wipeout.air_body_tweak[1]};
        } else {
            state.gesture = {0.0f, 0.0f};
        }
        return rs::Unit{};
    }
    if (action_controls.wipeout.request || static_cast<std::int32_t>(state.ticks) > 80) state.released = true;
    if (wipeout_controls.gestures_enabled) {
        S3_MHOST_SOME(const std::uint32_t animation_flags, animation.skater_animation_flags,
                       "Wipeout requires animation stance");
        const bool mirrored = (animation_flags & 0x40000000u) != 0;
        const motion_wipeout::Settings& s = wipeout_settings;
        const float threshold = s.threshold;
        const float twist_velocity = s.twist_velocity;
        const float twist_acceleration = s.twist_acceleration;
        const float lean_velocity = s.lean_velocity;
        const float twist_blend = s.twist_blend;
        const float lean_blend = s.lean_blend;
        const float gesture_y_blend = s.gesture_y_blend;
        const float gesture_x_blend = s.gesture_x_blend;
        const float lean_delta =
            ((1.0f - lean_blend) * state.lean + physical_.hips_right_angle_496 * lean_blend) - state.lean;
        state.lean += bounded(lean_delta, lean_velocity);
        set(animation, "Lean", state.lean);
        const float target = positive_turn(mirrored ? -physical_.hips_up_angle_500 : physical_.hips_up_angle_500);
        float delta = target - state.twist;
        const float pi = rs::from_bits(0x40490FDB);
        const float turn = rs::from_bits(0x40C90FDB);
        if (delta > pi) {
            delta -= turn;
        } else if (delta < -pi) {
            delta += turn;
        }
        const float blended =
            ((state.twist + delta) * twist_blend + (1.0f - twist_blend) * state.twist) - state.twist;
        float velocity = bounded(blended, twist_velocity);
        if (state.ticks >= 2) {
            velocity = bounded(velocity - state.twist_velocity, twist_acceleration) + state.twist_velocity;
        }
        float twist = state.twist + velocity;
        if (twist > turn) twist -= turn;
        state.twist_velocity = velocity;
        state.twist = positive_turn(twist);
        set(animation, "Twist", state.twist);
        const std::optional<std::array<float, 2>> gesture = action_controls.wipeout.gesture;
        const std::optional<float> x = gesture ? std::optional<float>((*gesture)[0]) : std::nullopt;
        const std::optional<float> y = gesture ? std::optional<float>((*gesture)[1]) : std::nullopt;
        const float xv = x.value_or(0.0f);
        const float yv = y.value_or(0.0f);
        if (!state.released && x && y && rs::abs(xv) < 0.1f && rs::abs(yv) < 0.1f) state.released = true;
        if (state.released) {
            state.gesture[0] = (1.0f - gesture_x_blend) * state.gesture[0] + xv * gesture_x_blend;
            state.gesture[1] = (1.0f - gesture_y_blend) * state.gesture[1] + yv * gesture_y_blend;
        }
        if (state.gesture[1] * state.gesture[1] + state.gesture[0] * state.gesture[0] > threshold * threshold) {
            animation.emit_packet(encode("ControlledWipeout"), 1.0f);
        }
        wipeout_controls.gesture = state.gesture;  //MGv2528258FB50.
    }
    set(animation, "WipeoutGestureX", state.gesture[0]);
    set(animation, "WipeoutGestureY", state.gesture[1]);
    state.ticks = state.ticks + 1u;  // wrapping_add
    return rs::Unit{};
}

rs::Result<rs::Unit, std::string> MotionHost::execute_twist_lean(BehaviorId behavior,
                                                                 motion_twist_lean::Operation operation,
                                                                 std::uint8_t phase) {
    if (behavior >= instances.size()) return rs::make_err(std::string("Unallocated MatchTwistAndLean behavior"));
    auto* state = instances[behavior].get<Instance::TwistLean>();
    if (state == nullptr) return rs::make_err(std::string("MatchTwistAndLean operation/instance mismatch"));
    std::optional<s3::player::offboard::twist_lean::Observation> observation;
    if (phase == 0 || (phase == 1 && operation.always)) {
        S3_MHOST_SOME(const motion_wipeout::Physical p, wipeout_physical,
                       "MatchTwistAndLean requires canonical physical angles");
        S3_MHOST_SOME(const std::uint32_t animation_flags, animation.skater_animation_flags,
                       "MatchTwistAndLean requires live animation stance");
        s3::player::offboard::twist_lean::Observation value{};
        value.hips_right_angle = p.hips_right_angle_496;
        value.hips_up_angle = p.hips_up_angle_500;
        value.mirrored = (animation_flags & 0x40000000u) != 0;
        observation = value;
    }
    operation.execute(state->_0, observation, &animation, phase);
    return rs::Unit{};
}

}  // namespace s3::game::graph_host::motion
