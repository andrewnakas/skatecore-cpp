// Port of skate-core/src/physics/drive_parameters.rs.
#include "s3/core/physics/drive_parameters.hpp"

#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::physics::drive_parameters {

namespace {
/// Original TU3 82C0B8A8..B8EC: two reciprocal-square-root refinements,
/// multiply by the spring, select zero for zero spring, then multiply by two.
/// The estimate uses our independent Rust arithmetic; hardware parity is unverified.
float soft_linear_damping() {
    const float spring = retail_f32(0x47C35000u);  // 822F91F0
    float reciprocal_root = native_arithmetic::reciprocal_square_root_estimate(spring);
    for (int i = 0; i < 2; ++i) {
        const float squared = reciprocal_root * reciprocal_root;
        const float half = reciprocal_root * 0.5f;
        const float residual = rs::mul_add(-spring, squared, 1.0f);
        reciprocal_root = rs::mul_add(half, residual, reciprocal_root);
    }
    const float root = spring == 0.0f ? 0.0f : spring * reciprocal_root;
    return root * retail_f32(0x40000000u);  // 82060C50
}
}  // namespace

RetailDriveDynamics retail_truck_drive_dynamics(RetailTruckDriveSettings settings) {
    RetailDriveParams linear;
    if (!settings.use_linear_drives) {
        linear = RetailDriveParams::DISABLED;
    } else if (settings.use_hard_linear_drives) {
        linear = RetailDriveParams{retail_f32(0x45BB7FFFu), 0.0f, retail_f32(0x48AFC7FFu),
                                   RetailDriveType::HardDrive};
    } else {
        linear = RetailDriveParams{
            retail_f32(0x47C35000u),
            // 82C0B858..B8EC: native sqrt(100000) refinement, then *2.
            soft_linear_damping(),
            retail_f32(0x48AFC7FFu),
            RetailDriveType::SoftDrive,
        };
    }
    return RetailDriveDynamics{
        linear,
        RetailDriveParams{
            settings.angular_displacement * retail_f32(0x426FFFFFu),
            settings.angular_damping,
            settings.angular_strength * retail_f32(0x4560FFFEu),
            RetailDriveType::HardDrive,
        },
    };
}

}  // namespace s3::physics::drive_parameters
