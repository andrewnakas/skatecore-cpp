// Port of the inline #[cfg(test)] module in skate-core/src/physics/drive_parameters.rs.
#include "harness.hpp"
#include "s3/core/physics/drive_parameters.hpp"

namespace {
using namespace s3::physics::drive_parameters;

S3_TEST(enabled_soft_linear_trucks_have_critical_damping) {
    RetailTruckDriveSettings settings = RetailTruckDriveSettings::STOCK;
    settings.use_linear_drives = true;
    settings.use_hard_linear_drives = false;
    const auto dynamics = retail_truck_drive_dynamics(settings);
    CHECK(dynamics.linear.drive_type == RetailDriveType::SoftDrive);
    CHECK_EQ(dynamics.linear.spring_or_max_velocity, 100000.0f);
    CHECK(s3::rs::abs(dynamics.linear.damping - 632.4555f) < 0.0001f);
    CHECK_EQ(s3::rs::to_bits(dynamics.linear.max_strength), 0x48AFC7FFu);
    CHECK_EQ(dynamics.angular, retail_truck_drive_dynamics(RetailTruckDriveSettings::STOCK).angular);
}
}  // namespace
