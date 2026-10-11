// Port of skate-core/src/player/offboard/movement_velocity.rs.
#include "s3/core/player/offboard/movement_velocity.hpp"

#include "s3/core/player/offboard/movement_velocity/math.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::player::offboard::movement_velocity {

using namespace movement_velocity::math;

void State::update(const Settings& settings, const Input& input) {
    const float dt = rs::from_bits(0x3c888889u);
    const float radians = rs::from_bits(0x3c8efa35u);
    const Vector projected = sub(input.forward, scale(input.plane_normal, dot(input.forward, input.plane_normal)));
    const Vector direction = normalize_or(projected, input.forward);
    // 82D7DB08..DB94 inlines the same recovered asin polynomial.
    const float slope_degrees = trigonometry::asin(direction[1]) * rs::from_bits(0x42652ee1u);
    float target_speed;
    if (input.slope_mode) {
        target_speed = settings.slope_mode_speed.evaluate(rs::abs(slope_degrees));
    } else {
        float target = settings.slope_speed_scalar.evaluate(slope_degrees) * input.desired_speed;
        if (!(input.desired_speed <= 0.5f) && !(target >= speed)) {
            const float gravity_delta =
                velocity[1] > 0.0f ? (rs::from_bits(0xc11ccccdu) * dt) * select(-direction[1], 0.0f, direction[1]) : 0.0f;
            const float delta = target - speed;
            const float limited_delta = select(delta - -2.0f, delta, -2.0f) * dt;
            target = speed + select(gravity_delta - limited_delta, limited_delta, gravity_delta);
        }
        target_speed = target;
    }
    const Vector original = velocity;
    Vector target = madd(original, 0.0f, scale(direction, target_speed));
    const float turn_error = rs::mul_add(settings.turn_vs_speed.evaluate(speed) * input.steering, radians, -turn);
    float maximum_delta = input.slope_mode ? 0.5f : rs::from_bits(0x3e4ccccdu);
    if (!(input.override_gate < 0.0f) && !input.slope_mode) {
        const bool first = override_remaining < 0.0f;
        if (first) override_remaining = input.override_duration;
        const float ratio =
            input.override_duration < rs::from_bits(0x3a83126fu) ? 0.0f : override_remaining / input.override_duration;
        const float amount = ratio * 1.25f;
        float blend = select(1.0f - amount, amount, 1.0f);
        if (!first && !(length(input.override_velocity) <= length(original) * rs::from_bits(0x3f8ccccdu))) {
            blend = select(blend - 0.5f, 0.5f, blend);
        }
        target = madd(original, blend, scale(input.override_velocity, 1.0f - blend));
        maximum_delta = 100.0f;
        if ((input.flags & 8u) != 0) {
            const float s = length(target);
            if (!(s >= rs::from_bits(0x3c23d70au))) {
                target = scale(direction, 1.0f);
            } else if (!(s >= 1.0f)) {
                target = scale(target, 1.0f / s);
            }
        }
        const float remaining = override_remaining - dt;
        override_remaining = select(-remaining, 0.0f, remaining);
    } else {
        override_remaining = -1.0f;
    }
    const float turn_delta = settings.turn_delta_vs_speed.evaluate(speed) * radians;
    Vector v = limit_delta(target, original, maximum_delta);
    turn += clamp(turn_error, -turn_delta, turn_delta);
    if (input.obstacle) {
        const Vector opposite = scale(input.obstacle_normal, -1.0f);
        target = remove_positive(target, opposite);
        v = remove_positive(v, opposite);
    }
    right_delta = dot(sub(v, original), input.right);
    forward_delta = dot(sub(target, original), input.forward);
    velocity = v;
    speed = length(v);
}

}  // namespace s3::player::offboard::movement_velocity
