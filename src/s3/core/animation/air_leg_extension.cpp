// Port of skate-core/src/animation/air_leg_extension.rs.
#include "s3/core/animation/air_leg_extension.hpp"

#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::animation::air_leg_extension {

using physics::native_arithmetic::dot3;

namespace {
float maximum(float a, float b) { return a - b >= 0.0f ? a : b; }
float minimum(float a, float b) { return b - a >= 0.0f ? a : b; }
}  // namespace

std::array<float, 2> State::update(Bone bone, Physical p, const Settings& settings, float dt,
                                   std::optional<AnimationAttribute> initial_attribute, bool prepare_to_land_) {
    const bool ascending = p.com_velocity[1] > 0.0f;
    switch (mode) {
        case 0: {
            mode = ascending ? 1 : 2;
            if (initial_attribute) {
                if (initial_attribute->kind == 0) {
                    //The queried union is initialized to zero before GetLastAttr.
                    height = rs::from_bits(initial_attribute->payload._0[0].value_or(0u));
                }
            } else {
                std::optional<std::array<float, 4>> toe;
                if (p.offboard_316 || bone == Bone::RightToe) {
                    toe = p.right_toe;
                } else if (bone == Bone::LeftToe) {
                    toe = p.left_toe;
                }
                if (toe) {
                    std::array<float, 4> d{};
                    for (std::size_t i = 0; i < 4; ++i) d[i] = p.com_position[i] - (*toe)[i];
                    height = dot3(d, p.system_up);
                } else {
                    height = p.animation_height;
                }
            }
            break;
        }
        case 1:
        case 2:
            if (mode == 1 && p.com_velocity[1] >= 0.0f) {
                //82BAFB60 is FNMSUBS, followed by FSEL against minimum1756.
                height = maximum(rs::mul_add(-dt, settings.going_up_speed, height), settings.minimum_height);
                break;
            }
            mode = 2;
            if (prepare_to_land_) {
                mode = 3;
                preland(bone, settings, dt);
            } else {
                //82BAFC88..FD0C: both dot products precede the fused blend.
                const float closing = -dot3(p.com_velocity, p.system_up);
                const float vertical = rs::abs(dot3(p.system_up, {0.0f, 1.0f, 0.0f, 0.0f}));
                float speed =
                    rs::mul_add(maximum(closing, 0.0f), vertical, (1.0f - vertical) * settings.going_down_speed);
                speed = minimum(speed, settings.going_down_speed);
                height = maximum(rs::mul_add(-speed, dt, height), settings.minimum_height);
            }
            break;
        case 3: preland(bone, settings, dt); break;
        default: break;
    }
    descending_time = ascending ? 0.0f : descending_time + dt;
    return {height, settings.arm_extension.evaluate(descending_time)};
}

void State::preland(Bone bone, const Settings& settings, float dt) {
    height = rs::mul_add(dt, settings.preland_velocity, height);
    if (bone != Bone::Board) {
        height *= settings.offboard_multiplier;
    }
}

bool prepare_to_land(bool physical_query, std::optional<AnimationAttribute> full_extension, Physical p,
                     const Settings& s) {
    if (!physical_query) {
        return false;
    }
    if (!full_extension) {
        return true;
    }
    //GetLastAttr's scalar output remains initialized zero for nonfloat records.
    const float extension = full_extension->kind == 0 ? rs::from_bits(full_extension->payload._0[0].value_or(0u)) : 0.0f;
    if (extension < s.preland_final_height - rs::from_bits(0x3dcccccd)) {
        return true;
    }
    const float required_time = (extension - p.animation_height) / s.preland_velocity;
    return p.remaining_air_time - rs::from_bits(0x3d75c28f) < required_time;
}

}  // namespace s3::animation::air_leg_extension
