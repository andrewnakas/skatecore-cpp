// Port of skate-core/src/player/wipeout_state/drives.rs (whole file).
#include "s3/core/player/wipeout_state/drives.hpp"

#include "s3/core/animation/foot_ik.hpp"
#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/physics/drive_parameters.hpp"

namespace s3::player::wipeout_state::drives {

using physics::board_motion_output::inverse_length_squared;
using physics::drive_parameters::RetailDriveParams;
using physics::drive_parameters::RetailDriveType;
using physics::skeleton_animation_record::IDENTITY;
using physics::skeleton_body::SkeletonDrives;

namespace {
const float FREQUENCY_SQUARED = rs::from_bits(0x4560FFFEu);
float clamp(float v) {
    const float lower = -v >= 0.0f ? 0.0f : v;
    return 1.0f - lower >= 0.0f ? lower : 1.0f;
}
}  // namespace

float update(SkeletonDrives& drives, const std::array<AnimationPartTransform, 24>& pose, const Settings& settings,
             Weights weights) {
    std::array<AnimationPartTransform, 24> inverses;
    inverses.fill(IDENTITY);
    for (std::size_t part = 1; part < 24; ++part) {
        inverses[part] = animation::foot_ik::inverse_affine(pose[part]);
    }
    const float residual = ((1.0f - weights.start) - weights.end) - weights.controlled;
    for (std::size_t part = 1; part < 23; ++part) {
        if (!drives.bones[part].has_value()) S3_PANIC("Original22 bone drives exist");
        auto& bone = *drives.bones[part];
        const auto coefficients = settings.bone[part];
        const bool upper = part >= 3 && part <= 10;
        const bool lower = part >= 15 && part <= 22;
        std::array<float, 2> strengths;
        if ((upper && weights.upper_extra > 0.0f) || (lower && weights.lower_extra > 0.0f)) {
            bone.dynamics.mode = 5;
            const float extra = upper ? weights.upper_extra : weights.lower_extra;
            const float value = extra * coefficients[4];
            strengths = {value * settings.strength[0], value * settings.strength[1]};
        } else {
            bone.dynamics.mode = 4;
            const float start = coefficients[0];
            const float normal = coefficients[1];
            const float controlled = coefficients[2];
            const float end = coefficients[3];
            // 82BEB424..444 preserves two distinct FMA accumulation orders.
            const float local = rs::mul_add(
                controlled, weights.controlled,
                rs::mul_add(start, weights.start, rs::mul_add(normal, residual, end * weights.end)));
            const float root_start = settings.root[0] * start;
            const float root_normal = settings.root[1] * normal;
            const float root_controlled = settings.root[2] * controlled;
            const float root_end = settings.root[3] * end;
            const float root = rs::mul_add(
                root_controlled, weights.controlled,
                rs::mul_add(root_start, weights.start, rs::mul_add(root_end, weights.end, root_normal * residual)));
            strengths = {local * settings.strength[0], root * settings.strength[1]};
        }
        bone.dynamics.strengths = strengths;
        for (std::size_t channel = 0; channel < 2; ++channel) {
            if (!bone.active[channel]) {
                continue;
            }
            bone.frames[channel] =
                physics::skeleton_body::bone_drive_frames(pose[part], inverses[part], inverses[bone.parent[channel]]);
            bone.dynamics.enable(channel, strengths[channel], drives.settings.bone);
        }
    }
    return residual;
}

void set_linear_root(SkeletonDrives& drives, const Settings& s, float weight) {
    weight = clamp(weight);
    const float spring = s.hook_spring;
    const float root = spring == 0.0f ? 0.0f : spring * inverse_length_squared(spring, 2);
    drives.targets.dynamics[0].linear = RetailDriveParams{
        spring * weight,
        root * 2.0f - spring * rs::from_bits(0x3C83126Fu),
        (s.hook_strength * weight) * FREQUENCY_SQUARED,
        RetailDriveType::SoftDrive,
    };
}

void set_angular_root(SkeletonDrives& drives, const Settings& s, float weight) {
    weight = clamp(weight);
    drives.targets.dynamics[0].angular = RetailDriveParams{
        s.hook_spring * weight,
        s.hook_damping,
        (s.hook_strength * weight) * FREQUENCY_SQUARED,
        RetailDriveType::SoftDrive,
    };
}

}  // namespace s3::player::wipeout_state::drives
