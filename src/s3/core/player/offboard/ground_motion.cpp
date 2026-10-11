// Port of skate-core/src/player/offboard/ground_motion.rs and its approach/publication/
// support submodules.
#include "s3/core/player/offboard/ground_motion.hpp"

#include "s3/core/player/offboard/ground_motion/math.hpp"

namespace s3::player::offboard::ground_motion {

using namespace ground_motion::math;

void update(GroundMotionState& state, const GroundMotionInput& input) {
    const Frame support_delta = support::update(state, input);
    const Vector old_position = state.frame_0[3];
    state.frame_0[3] = ZERO;
    const auto [step, height] = approach::calculate(state, input, old_position, support_delta);
    Vector displacement = add(ZERO, step);
    displacement = add(displacement, height);
    displacement = add(displacement, input.contact_displacement_384);
    displacement = madd(input.velocity_addition_528, DT, displacement);
    displacement = madd(state.predicted_support_velocity_272, DT, displacement);
    const float angle = (state.angular_velocity_688 + state.predicted_support_yaw_344) * DT;
    const Vector axis = state.frame_0[1];
    for (auto& row : state.frame_0) row = rotate(row, axis, angle);
    state.frame_0[0] = normalize(cross(input.desired_up_544, state.frame_0[2]));
    state.frame_0[1] = input.desired_up_544;
    state.frame_0[2] = normalize(cross(state.frame_0[0], input.desired_up_544));
    state.frame_0 = physics::skeleton_root::orthonormalize(state.frame_0);
    state.frame_0[3] = add(old_position, displacement);
    publication::update(state, displacement);
}

// ---- approach.rs: 82D7F458..FDD0 animation-directed movement and ground/obstacle approach ----
namespace approach {
namespace {
/// 82D80EB0. No state change when the job has no target frame.
void refresh_target(GroundMotionState& s, const GroundMotionInput& i) {
    if (i.target_frame_present_352) {
        s.target_frame_608 = i.target_frame_368;
        s.target_frame_608[3] = madd(i.target_frame_368[0], rs::from_bits(0x3e4ccccdu), i.target_frame_368[3]);
    }
}
Vector height_adjustment(const GroundMotionState& s, const GroundMotionInput& i, Vector position) {
    return scale(s.frame_0[1], dot(sub(i.contact_position_0, position), s.frame_0[1]) * rs::from_bits(0x3d4ccccdu));
}
}  // namespace

std::pair<Vector, Vector> calculate(GroundMotionState& s, const GroundMotionInput& i, Vector position, const Frame& delta) {
    Vector step = scale(s.velocity_480, DT);
    Vector height = ZERO;
    if (i.animation_directed_304) {
        const float animation_speed = length(i.animation_motion_224);
        const bool stationary = animation_speed < EPSILON;
        Vector difference = sub(s.target_frame_608[3], position);
        float distance = length(difference);
        if (!(s.target_scale_784 >= 0.0f)) {
            refresh_target(s, i);
            difference = sub(s.target_frame_608[3], position);
            distance = length(difference);
            s.target_scale_784 = distance / (stationary ? i.requested_duration_288 : animation_speed);
        }
        step = ZERO;
        if (!(distance <= EPSILON)) {
            float step_length = s.target_scale_784 * DT;
            if (!stationary) step_length = length(i.animation_motion_240) * step_length;
            step = scale(scale(difference, reciprocal(distance)), step_length);
        }
        const float sine = clamp_signed(dot(cross(s.target_frame_608[0], s.frame_0[2]), s.frame_0[1]));
        const float angle = trigonometry::asin(sine);
        const float denominator =
            stationary ? i.requested_duration_288 * s.target_scale_784 : s.target_scale_784 * animation_speed;
        s.angular_velocity_688 = 0.0f;
        if (!(denominator <= EPSILON)) {
            s.angular_velocity_688 = ((1.0f - distance / denominator) * angle) * RATE;
        }
        s.target_frame_608 = compose(s.target_frame_608, delta);
        return {step, height};
    }
    s.target_scale_784 = -1.0f;
    refresh_target(s, i);
    if ((i.contact_flags_176 & 2u) == 0 && !i.obstacle_enabled_713) {
        if ((i.contact_flags_176 & 1u) != 0) height = height_adjustment(s, i, position);
        return {step, height};
    }
    Vector target;
    if (i.obstacle_enabled_713) {
        target = i.obstacle_target_672;
    } else {
        const Vector side = normalize_safe(cross(UP, s.frame_0[2]), s.frame_0[0]);
        const Vector point = s.correction_enabled_711 ? i.correction_target_592 : i.contact_target_96;
        target = sub(point, scale(side, dot(sub(point, position), side)));
    }
    Vector difference = sub(target, position);
    if (dot(difference, difference) < rs::from_bits(0x37800000u) || !(dot(difference, s.velocity_480) >= -0.5f)) {
        const Vector predicted = madd(scale(s.velocity_480, DT), 2.0f, position);
        target = Vector{predicted[0], target[1], predicted[2], predicted[3]};
    }
    difference = sub(target, position);
    const float budget = length(step);
    const float distance = length(difference);
    if (!(distance <= budget)) {
        step = scale(normalize_safe(difference, ZERO), budget);
    } else {
        Vector normal = i.obstacle_enabled_713 ? i.target_frame_368[1] : i.contact_normal_112;
        const float angle = rs::from_bits(0x42340000u) * rs::from_bits(0x3c8efa35u);
        const Vector limited = limit_angle(normal, i.reference_frame_128[1], angle);
        normal = normalize_safe(limited, ZERO);
        const Vector tangent = sub(s.velocity_480, scale(normal, dot(s.velocity_480, normal)));
        step = madd(normalize_safe(tangent, ZERO), budget - distance, difference);
    }
    if (!(budget >= rs::from_bits(0x3c23d70au))) height = height_adjustment(s, i, position);
    return {step, height};
}
}  // namespace approach

// ---- publication.rs ----
namespace publication {
void update(GroundMotionState& s, Vector displacement) {
    s.published_frame_64 = s.frame_0;
    if (!s.correction_enabled_711) return;
    const float magnitude = length(s.correction_576);
    if (magnitude <= rs::from_bits(0x3c23d70au)) {
        s.correction_576 = ZERO;
        s.correction_enabled_711 = false;
    } else {
        const Vector unit = scale(s.correction_576, 1.0f / magnitude);
        const float proportional = magnitude * rs::from_bits(0xbd4ccccdu);
        const float minimum = rs::from_bits(0xbba3d70au);
        const float decay = select(minimum - proportional, proportional, minimum);
        const float projected = dot(displacement, unit);
        const float amount = select(projected - decay, decay, projected);
        if (amount > -magnitude) {
            s.correction_576 = madd(unit, amount, s.correction_576);
        } else {
            s.correction_576 = ZERO;
            s.correction_enabled_711 = false;
        }
    }
    s.published_frame_64[3] = sub(s.published_frame_64[3], s.correction_576);
}
}  // namespace publication

// ---- support.rs ----
namespace support {
namespace {
/// 82D800C0: reference-frame local acceleration, mirrored X/Z, cap then filter.
void filter(GroundMotionState& s, const GroundMotionInput& i) {
    const Frame& f = i.reference_frame_128;
    const Frame columns{{{f[0][0], f[1][0], f[2][0], 0.0f}, {f[0][1], f[1][1], f[2][1], 0.0f},
                         {f[0][2], f[1][2], f[2][2], 0.0f}, ZERO}};
    Vector local = transform_vector(s.support_acceleration_304, columns);
    local[1] = 0.0f;
    if (i.mirrored_292) {
        local[0] *= -1.0f;
        local[2] *= -1.0f;
    }
    Vector request = scale(local, rs::from_bits(0x3ccccccdu));
    const float magnitude = length(request);
    if (!(magnitude <= 1.0f)) request = scale(request, 1.0f / magnitude);
    s.filtered_local_acceleration_320 = madd(request, rs::from_bits(0x3d23d70au),
                                             scale(s.filtered_local_acceleration_320, rs::from_bits(0x3f75c28fu)));
    const float speed = length(Vector{s.support_velocity_256[0], 0.0f, s.support_velocity_256[2], s.support_velocity_256[0]}) *
                        rs::from_bits(0x3d4ccccdu);
    const float nonnegative = select(-speed, 0.0f, speed);
    s.support_speed_348 = select(1.0f - nonnegative, nonnegative, 1.0f);
}
}  // namespace

Frame update(GroundMotionState& s, const GroundMotionInput& i) {
    Frame delta = IDENTITY;
    if ((i.contact_flags_176 & 1u) == 0) return delta;
    if (s.support_id_352 != i.contact_id_180) {
        if (s.support_velocity_removed_715) {
            s.velocity_480 = add(s.velocity_480, s.predicted_support_velocity_272);
            s.speed_704 = length(s.velocity_480);
        }
        s.support_yaw_336 = 0.0f;
        s.previous_support_yaw_340 = 0.0f;
        s.predicted_support_yaw_344 = 0.0f;
        s.support_acceleration_304 = ZERO;
        s.predicted_support_velocity_272 = ZERO;
        s.support_velocity_256 = ZERO;
        s.support_id_352 = i.contact_id_180;
        s.support_velocity_removed_715 = false;
    } else {
        delta = compose(inverse_rigid(s.previous_support_frame_192), i.contact_frame_32);
        const Vector moved = transform_point(s.frame_0[3], delta);
        const Vector velocity = scale(sub(moved, s.frame_0[3]), reciprocal(DT));
        const Vector acceleration = madd(sub(velocity, s.support_velocity_256), reciprocal(DT), s.support_acceleration_304);
        s.support_acceleration_304 = scale(acceleration, rs::from_bits(0x3f733333u));
        s.support_velocity_256 = velocity;
        s.predicted_support_velocity_272 = add(velocity, sub(velocity, s.previous_support_velocity_288));
        s.previous_support_velocity_288 = velocity;
        const Vector rotated = transform_vector(s.frame_0[2], delta);
        const Vector flat = sub(rotated, scale(UP, dot(rotated, UP)));
        const Vector direction = normalize_safe(flat, ZERO);
        const float sine = clamp_signed(dot(cross(s.frame_0[2], direction), UP));
        const float yaw = trigonometry::asin(sine) * RATE;
        s.support_yaw_336 = yaw;
        s.predicted_support_yaw_344 = rs::mul_add(yaw, 2.0f, -s.previous_support_yaw_340);
        s.previous_support_yaw_340 = yaw;
        if (dot(s.predicted_support_velocity_272, s.predicted_support_velocity_272) > EPSILON &&
            !s.support_velocity_removed_715) {
            s.velocity_480 = add(s.velocity_480, rs::map(s.predicted_support_velocity_272, [](float v) { return -v; }));
            s.support_velocity_removed_715 = true;
            s.speed_704 = length(s.velocity_480);
        }
    }
    filter(s, i);
    s.previous_support_frame_192 = i.contact_frame_32;
    return delta;
}
}  // namespace support

}  // namespace s3::player::offboard::ground_motion
