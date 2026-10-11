// Port of skate-core/src/physics/skeleton_landing.rs.
#include "s3/core/physics/skeleton_landing.hpp"

namespace s3::physics::skeleton_landing {

namespace {
float select(float selector, float positive, float negative) { return selector >= 0.0f ? positive : negative; }
}  // namespace

void LandingAdjustment::update(LandingInput input, const LandingSettings& settings, SkateboardOffset& offset) {
    const bool from_offboard = previous_filtered_state == 6 && input.filtered_state == 1;
    const bool from_air = previous_filtered_state == 2 && input.filtered_state == 1;
    const bool ground = (from_offboard || from_air) && (input.flags_2476 & 0x10000000u) != 0;
    const bool manual = from_air && input.balance != 0.0f;
    const bool grind =
        previous_filtered_state == 2 && input.filtered_state == 3 && (input.flags_2468 & 0x20u) == 0;
    const bool coffin_flag = (input.flags_2472 & 1u) != 0;
    const bool coffin = coffin_flag && (grind || from_air);
    const bool cancel = (input.flags_2476 & 0x40000000u) != 0 && !coffin_flag;
    if (cancel) {
        active = false;
    }
    bool first_ground_update = false;
    if (!cancel && !active && (ground || coffin || manual || grind)) {
        time = 0.0f;
        active = true;
        kind = coffin ? 3u : manual ? 2u : ground ? 0u : 1u;
        first_ground_update = kind == 0;
        const float v = (grind || coffin) ? previous_com_velocity : input.physical_com_velocity_along_up;
        const float lower = select(-settings.maximum_velocity - v, -settings.maximum_velocity, v);
        velocity = select(-lower, lower, 0.0f);
        previous_animation_height = rs::f32::MAX;
        position = input.physical_com_height;
    }
    if (active) {
        float spring = 0.0f;
        float damping = 0.0f;
        float blend = 0.0f;
        bool release = false;
        bool allow_upwards = true;
        switch (kind) {
            case 0:
                spring = settings.ground_spring;
                damping = settings.ground_damping;
                allow_upwards = false;
                blend = 1.0f;
                release = input.filtered_state != 1 || (input.flags_2476 & 0x10000000u) == 0;
                break;
            case 1:
                spring = settings.grind_spring;
                damping = settings.grind_damping;
                blend = settings.grind_blend.evaluate(time);
                release = input.filtered_state != 3;
                break;
            case 2:
                spring = settings.manual_spring;
                damping = settings.manual_damping;
                blend = settings.manual_blend.evaluate(time);
                release = input.filtered_state != 1;
                break;
            case 3:
                blend = 1.0f;
                release = input.filtered_state != 1 || time > settings.coffin_time || !coffin_flag;
                break;
            default:
                break;
        }
        const float animation_height = input.animation_com_height;
        if (kind == 3) {
            const float ratio = rs::abs(velocity) / settings.coffin_maximum_velocity;
            const float positive_ratio = select(-ratio, 0.0f, ratio);
            const float scale = select(1.0f - positive_ratio, positive_ratio, 1.0f);
            const float height = settings.coffin_height.evaluate(time / settings.coffin_time);
            position = rs::mul_add(height, scale, settings.coffin_base_height);
        } else {
            float target = settings.desired_com_height;
            if (kind == 1) {
                if (time > settings.grind_animation_target_time) {
                    const float minimum = desired_grind_com - settings.grind_target_delta;
                    const float maximum = desired_grind_com + settings.grind_target_delta;
                    const float lower = select(minimum - animation_height, minimum, animation_height);
                    target = select(maximum - lower, lower, maximum);
                }
                desired_grind_com = target;
            }
            const float displacement = position - target;
            const float previous_velocity = velocity;
            // fnmsubs of damping*velocity and negative spring displacement.
            const float spring_force = -(displacement * spring);
            const float acceleration = -(rs::mul_add(previous_velocity, damping, -spring_force));
            const float candidate = rs::mul_add(acceleration, STEP, previous_velocity);
            const float lower = select(-settings.maximum_velocity - candidate, -settings.maximum_velocity, candidate);
            const float maximum = allow_upwards ? settings.maximum_velocity : 0.0f;
            velocity = select(maximum - lower, lower, maximum);
            const float integrated = rs::mul_add((velocity + previous_velocity) * 0.5f, STEP, displacement);
            const float height = integrated + target;
            position = select(height - settings.minimum_height, height, settings.minimum_height);
        }
        float displacement = animation_height - position;
        if (first_ground_update) {
            const float correction = velocity * rs::from_bits(0x3C75C28Fu);
            const float lower = select(-0.1f - correction, -0.1f, correction);
            const float down = select(-lower, lower, 0.0f);
            const float combined = down + displacement;
            displacement = select(combined, combined, 0.0f);
        }
        if (kind == 0 && time > settings.ground_minimum_compression_time &&
            previous_animation_height < animation_height && displacement > 0.0f) {
            release = true;
        }
        if (kind == 1 && (input.flags_2468 & 0x20u) != 0) {
            release = true;
        }
        if (time > 1.5f) {
            release = true;
        }
        const bool force_blend = (input.flags_2468 & (1u << 21)) != 0;
        if ((!release && blend >= 0.05f) || force_blend) {
            float frames = kind == 3 ? settings.coffin_blend_frames : 15.0f;
            if (force_blend) {
                active = false;
                frames = 6.0f;
            }
            offset.refresh_height(displacement * blend, frames);
        } else {
            active = false;
        }
        time += STEP;
        previous_animation_height = animation_height;
    }
    previous_com_velocity = input.physical_com_velocity_along_up;
    previous_filtered_state = input.filtered_state;
}

}  // namespace s3::physics::skeleton_landing
