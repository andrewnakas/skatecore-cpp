// Port of skate-core/src/physics/skeleton_body/drive_dynamics.rs.
#include "s3/core/physics/skeleton_body.hpp"

namespace s3::physics::skeleton_body::drive_dynamics {

namespace {

constexpr float FREQUENCY = rs::from_bits(0x426FFFFFu);
constexpr float FREQUENCY_SQUARED = rs::from_bits(0x4560FFFEu);
constexpr float HARD_VELOCITY = rs::from_bits(0x4415FFFFu);
constexpr float HARD_STRENGTH = rs::from_bits(0x470C9FFFu);

RetailDriveParams hard(float velocity, float strength) {
    return RetailDriveParams{velocity, 0.0f, strength, RetailDriveType::HardDrive};
}
RetailDriveParams soft(float spring, float damping, float strength) {
    return RetailDriveParams{spring, damping, strength, RetailDriveType::SoftDrive};
}
RetailDriveParams interpolate(DriveInterpolation settings, float strength, float progress) {
    // Original82BED100..114 are scalar fmadds (primary59,extended29).
    // The retained older pseudocode expands them to multiply+add expressions.
    const float spring0 = settings.spring[0] * strength;
    const float strength0 = settings.strength[0] * strength;
    const float spring = rs::mul_add(settings.spring[1] * strength - spring0, progress, spring0);
    const float force = rs::mul_add(settings.strength[1] * strength - strength0, progress, strength0);
    const float damping = rs::mul_add(settings.damping[1] - settings.damping[0], progress, settings.damping[0]);
    auto nonnegative = [](float value) { return value >= 0.0f ? value : 0.0f; };
    RetailDriveParams out;
    out.spring_or_max_velocity = nonnegative(spring) * FREQUENCY;
    out.max_strength = nonnegative(force) * FREQUENCY_SQUARED;
    out.damping = nonnegative(damping);
    out.drive_type = RetailDriveType::HardDrive;
    return out;
}

}  // namespace

void BoneDriveDynamics::enable(std::size_t channel, float strength, BoneDriveSettings settings) {
    S3_ASSERT(channel < 2);
    if (mode > 5) {
        return;
    }
    if (mode != 5) {
        transition_active = false;
    }
    const AnimationDriveSettings animation = settings.animation[channel];
    RetailDriveDynamics dynamics;
    switch (mode) {
        case 0:
            dynamics = RetailDriveDynamics{
                hard(animation.linear_displacement * FREQUENCY, animation.linear_strength * FREQUENCY_SQUARED),
                hard(animation.angular_displacement * FREQUENCY, animation.angular_strength * FREQUENCY_SQUARED),
            };
            break;
        case 1:
            dynamics = RetailDriveDynamics{hard(HARD_VELOCITY, HARD_STRENGTH), hard(HARD_VELOCITY, HARD_STRENGTH)};
            break;
        case 2:
            dynamics = RetailDriveDynamics{
                hard(0.0f, 0.0f),
                hard(animation.angular_displacement * FREQUENCY, animation.angular_strength * FREQUENCY_SQUARED),
            };
            break;
        case 3:
            dynamics = RetailDriveDynamics{
                soft(0.0f, 0.0f, 0.0f),
                soft(settings.collision_soft_displacement * strength, 200.0f,
                     (settings.collision_soft_strength * strength) * FREQUENCY_SQUARED),
            };
            break;
        case 4:
            dynamics = RetailDriveDynamics{
                strength <= 0.5f ? soft(0.0f, 0.0f, 0.0f)
                                 : soft((settings.ragdoll_soft_displacement * strength) * 0.5f, 200.0f,
                                        (strength * settings.ragdoll_soft_strength) * FREQUENCY_SQUARED),
                soft(settings.ragdoll_soft_displacement * strength, 200.0f,
                     (strength * settings.ragdoll_soft_strength) * FREQUENCY_SQUARED),
            };
            break;
        case 5: {
            if (!transition_active) {
                transition_active = true;
                transition_counter = 1.0f;
            } else if (transition_counter < settings.transition_calls) {
                transition_counter += 1.0f;
            }
            const float progress = transition_counter / settings.transition_calls;
            dynamics = RetailDriveDynamics{
                interpolate(settings.transition_linear, strength, progress),
                interpolate(settings.transition_angular, strength, progress),
            };
            break;
        }
        default:
            S3_PANIC("internal error: entered unreachable code");
    }
    channels[channel] = dynamics;
}

}  // namespace s3::physics::skeleton_body::drive_dynamics
