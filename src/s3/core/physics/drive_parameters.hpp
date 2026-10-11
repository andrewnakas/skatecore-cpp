// Port of skate-core/src/physics/drive_parameters.rs.
// Recovered drive parameter layout and TU3 truck/wheel construction branches.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/physics/f32_bits.hpp"
#include "s3/core/rs.hpp"

namespace s3::physics::drive_parameters {

constexpr float retail_f32(std::uint32_t bits) { return f32_from_bits(bits); }

enum class RetailDriveType : std::uint32_t {
    NoDrive = 0,
    SoftDrive = 1,
    HardDrive = 2,
};

struct RetailDriveParams {
    float spring_or_max_velocity = 0.0f;
    float damping = 0.0f;
    float max_strength = 0.0f;
    RetailDriveType drive_type = RetailDriveType::NoDrive;

    static const RetailDriveParams DISABLED;
};
inline constexpr RetailDriveParams RetailDriveParams::DISABLED =
    RetailDriveParams{0.0f, 0.0f, 0.0f, RetailDriveType::NoDrive};
S3_DERIVE_EQ(RetailDriveParams, spring_or_max_velocity, damping, max_strength, drive_type)

struct RetailDriveDynamics {
    RetailDriveParams linear;
    RetailDriveParams angular;
};
S3_DERIVE_EQ(RetailDriveDynamics, linear, angular)

/// Separate collections read by TU3 CreateDrives and SetTruckDriveDynamics.
/// `physicstrucks_drives.UseSoftDrives` is not read by this TU3 path.
struct RetailTruckDriveSettings {
    /// physicstrucks layout +0; gates only the linear configuration.
    bool use_linear_drives = false;
    /// physicsdeck full attribute 103FDC19514D4246; read only when enabled.
    bool use_hard_linear_drives = false;
    /// physicstrucks_drives layout +24, +28 and +20 respectively.
    float angular_displacement = 0.0f;
    float angular_damping = 0.0f;
    float angular_strength = 0.0f;

    /// Decoded default collections, not truck geometry/twist-limit fields.
    static const RetailTruckDriveSettings STOCK;
};
inline constexpr RetailTruckDriveSettings RetailTruckDriveSettings::STOCK =
    RetailTruckDriveSettings{false, true, 10.0f, 0.0f, 11.0f};
S3_DERIVE_EQ(RetailTruckDriveSettings, use_linear_drives, use_hard_linear_drives,
             angular_displacement, angular_damping, angular_strength)

/// Constructor 82C06298 zeros both halves at +7152/+7168, then CreateDrives
/// 82C0B770 creates the truck pair and applies these settings. Postphysics only
/// refreshes frames in TU3; it does not rerun this parameter setup every tick.
RetailDriveDynamics retail_truck_drive_dynamics(RetailTruckDriveSettings settings);

constexpr RetailDriveDynamics retail_wheel_drive_dynamics(bool use_hard_drives) {
    if (use_hard_drives) {
        return RetailDriveDynamics{
            RetailDriveParams{retail_f32(0x45BB7FFFu), 0.0f, retail_f32(0x48AFC7FFu),
                              RetailDriveType::HardDrive},
            RetailDriveParams{0.0f, 0.0f, 0.0f, RetailDriveType::NoDrive},
        };
    }
    return RetailDriveDynamics{
        RetailDriveParams{retail_f32(0x45610000u), retail_f32(0x42700000u), retail_f32(0x470C9FFFu),
                          RetailDriveType::SoftDrive},
        RetailDriveParams{0.0f, 0.0f, 0.0f, RetailDriveType::NoDrive},
    };
}

struct RetailDriveFrameRaw {
    /// Retail quaternion in observed `(x, y, z, w)` lane order.
    std::array<std::uint32_t, 4> quaternion_lanes{};
    /// Affine translation vector copied from matrix offset `0x30`.
    std::array<std::uint32_t, 4> translation_lanes{};
};
S3_DERIVE_EQ(RetailDriveFrameRaw, quaternion_lanes, translation_lanes)

struct RetailDriveFramesRaw {
    /// Frame emitted by helper `0x82BD3A10`.
    RetailDriveFrameRaw body_a;
    /// Frame emitted by helper `0x82BD3BD0`.
    RetailDriveFrameRaw body_b;
};
S3_DERIVE_EQ(RetailDriveFramesRaw, body_a, body_b)

}  // namespace s3::physics::drive_parameters
