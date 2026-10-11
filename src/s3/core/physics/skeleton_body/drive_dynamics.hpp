// Port of skate-core/src/physics/skeleton_body/drive_dynamics.rs (included via skeleton_body.hpp).
// BoneDrives::EnableDrive82BECCE0 and mode-five interpolation82BECFB8.
// A bone shares its interpolation counter between the LOCAL and LOCAL_ROOT
// channels. Dispatch order is therefore observable even within one tick.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "s3/core/physics/drive_parameters.hpp"

namespace s3::physics::skeleton_body::drive_dynamics {

using drive_parameters::RetailDriveDynamics;
using drive_parameters::RetailDriveParams;
using drive_parameters::RetailDriveType;

struct AnimationDriveSettings {
    float linear_strength = 0.0f;
    float linear_displacement = 0.0f;
    float angular_strength = 0.0f;
    float angular_displacement = 0.0f;
};

struct DriveInterpolation {
    std::array<float, 2> spring{};
    std::array<float, 2> strength{};
    std::array<float, 2> damping{};
};

struct BoneDriveSettings {
    std::array<AnimationDriveSettings, 2> animation{};
    float collision_soft_displacement = 0.0f;
    float collision_soft_strength = 0.0f;
    float ragdoll_soft_displacement = 0.0f;
    float ragdoll_soft_strength = 0.0f;
    DriveInterpolation transition_linear;
    DriveInterpolation transition_angular;
    float transition_calls = 0.0f;
};

struct BoneDriveDynamics {
    std::array<RetailDriveDynamics, 2> channels{
        RetailDriveDynamics{RetailDriveParams::DISABLED, RetailDriveParams::DISABLED},
        RetailDriveDynamics{RetailDriveParams::DISABLED, RetailDriveParams::DISABLED},
    };
    std::uint32_t mode = 0;
    std::array<float, 2> strengths{};
    float transition_counter = 0.0f;
    bool transition_active = false;

    static BoneDriveDynamics default_() { return BoneDriveDynamics{}; }

    void enable(std::size_t channel, float strength, BoneDriveSettings settings);
};

}  // namespace s3::physics::skeleton_body::drive_dynamics

namespace s3::physics::skeleton_body {
using drive_dynamics::AnimationDriveSettings;
using drive_dynamics::BoneDriveDynamics;
using drive_dynamics::BoneDriveSettings;
using drive_dynamics::DriveInterpolation;
}  // namespace s3::physics::skeleton_body
