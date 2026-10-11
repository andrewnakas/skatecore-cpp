// Port of skate-core/src/player/offboard/movement_intent.rs.
#include "s3/core/player/offboard/movement_intent.hpp"

#include "s3/core/player/offboard/movement_intent/math.hpp"

namespace s3::player::offboard::movement_intent {

using namespace movement_intent::math;

void State::update(const Settings& settings, const Input& input) {
    const float minimum = (!input.suppress_minimum && (input.flags & 8u) != 0) ? 0.5f : 0.0f;
    const float magnitude = select(minimum - input.magnitude, minimum, input.magnitude);
    const float dt = rs::from_bits(0x3c888889u);
    sprint_grace = input.sprint_pressed ? rs::from_bits(0x3e99999au) : sprint_grace - dt;
    const bool sprint = (sprint_grace > 0.0f || input.sprint_pressed) && !input.edge_active;
    sprint_time = clamp(sprint_time + (sprint ? dt : -dt), 0.0f, settings.sprint_time_cap);
    const float fast = settings.sprint_speed.evaluate(magnitude);
    const float normal = settings.normal_speed.evaluate(magnitude);
    const float blend = sprint ? settings.sprint_blend.evaluate(sprint_time) : 0.0f;
    speed = rs::mul_add(1.0f - blend, normal, blend * fast);
    secondary_speed = 0.0f;
    steering = input.steering;
    original_steering = input.steering;
    edge_aligned = false;
    if (input.obstacle && !input.ignore_obstacle && !(speed <= 0.0f)) {
        const Vector opposite = scale(input.obstacle_normal, -1.0f);
        const float desired = wrap_angle(signed_angle(opposite, input.direction, Vector{0.0f, 1.0f, 0.0f, 0.0f}));
        const float degrees = obstacle_centered ? 55.0f : 25.0f;
        obstacle_centered = degrees * rs::from_bits(0x3c8efa35u) > rs::abs(desired);
        const float facing = wrap_angle(signed_angle(opposite, input.forward, Vector{0.0f, 1.0f, 0.0f, 0.0f}));
        if (!(rs::abs(facing) >= HALF_PI)) {
            const float correction =
                obstacle_centered
                    ? (facing * rs::from_bits(0xbf22f983u)) * 0.5f
                    : rs::mul_add(-facing, rs::from_bits(0x3f22f983u), desired <= 0.0f ? -1.0f : 1.0f) * 0.5f;
            merge_steering(clamp(correction, -1.0f, 1.0f));
            if (obstacle_centered) speed = 0.0f;
        }
    } else {
        obstacle_centered = false;
        if (input.sliding) {
            apply_slide(settings, input);
        } else if (input.edge_active && !(speed <= 0.0f)) {
            const Vector direction = input.magnitude == 0.0f ? input.forward : input.direction;
            const Vector tangent =
                dot(input.edge_tangent, direction) >= 0.0f ? input.edge_tangent : scale(input.edge_tangent, -1.0f);
            edge_target = madd(tangent, 0.5f, input.edge_point);
            const Vector delta = sub(edge_target, input.position);
            if (!(dot(horizontal(direction), horizontal(delta)) <= rs::from_bits(0x3f666666u))) {
                edge_aligned = true;
                const Vector normalized = normalize_or(delta, input.forward);
                steering = clamp(dot(normalized, input.right) * 0.5f, -1.0f, 1.0f);
            }
        }
    }
}

void State::merge_steering(float correction) {
    if (!(steering * correction >= 0.0f)) {
        steering = correction;
    } else {
        const float sign = correction <= 0.0f ? -1.0f : 1.0f;
        const float old = steering * sign;
        const float new_ = correction * sign;
        steering = select(old - new_, old, new_) * sign;
    }
}

void State::apply_slide(const Settings& settings, const Input& input) {
    const float angle = wrap_angle(projected_angle(input.slide_velocity, input.forward, input.up));
    const float original = steering;
    speed = (1.0f - rs::abs(original)) * speed;
    const float sign = select(angle, 1.0f, -1.0f);
    if (!(original * sign <= 0.0f)) {
        const float limit = 45.0f * rs::from_bits(0x3c8efa35u);
        const float weight = rs::mul_add(-angle, sign, limit) / limit;
        steering = clamp(weight, 0.0f, 1.0f) * original;
    }
    const float correction = settings.slide_steering.evaluate(sign * (angle * rs::from_bits(0x3ea2f983u)));
    steering = clamp(rs::mul_add(-correction, sign, steering), -1.0f, 1.0f);
    if (!(rs::abs(angle) >= HALF_PI)) speed += length(input.slide_velocity);
}

}  // namespace s3::player::offboard::movement_intent
