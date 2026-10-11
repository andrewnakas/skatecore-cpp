// Port of skate-core/src/physics/joint_records.rs.
#include "s3/core/physics/joint_records.hpp"

#include "s3/core/trigonometry.hpp"

namespace s3::physics::joint_records {

using drive_frames::AuthoredTransformInputs;
using drive_frames::RetailDriveFrame;
using drive_frames::RetailDriveFrames;
using math::Basis3;
using math::Vector3;
using rigid_body::RetailQuaternion;

namespace {

constexpr float DEGREES_TO_RADIANS = f32_from_bits(0x3C8EFA35u);
constexpr float SIMULATION_FREQUENCY = f32_from_bits(0x426FFFFFu);
constexpr float HALF_PI = f32_from_bits(0x3FC90FDBu);

RetailJointParametersRaw truck_parameters(RetailJointSettings settings) {
    const float angle_radians = settings.truck_twist_angle_degrees * DEGREES_TO_RADIANS;
    const float angle_cosine = trigonometry::sin_cos(angle_radians).second;
    const float twist_velocity = (settings.truck_twist_limit_degrees * DEGREES_TO_RADIANS) * SIMULATION_FREQUENCY;
    std::array<std::uint32_t, 16> words{};
    words[8] = rs::to_bits(twist_velocity);
    // The native parameter block retains the angular boundary in its spare
    // fourth lane while the solver consumes cos(angle) from word 13.
    words[11] = rs::to_bits(angle_radians);
    words[12] = rs::to_bits(1.0f);
    words[13] = rs::to_bits(angle_cosine);
    words[14] = 0;
    words[15] = 1;
    return RetailJointParametersRaw{words};
}

RetailJointParametersRaw wheel_parameters(RetailJointSettings settings) {
    const float swing_velocity = (settings.wheel_swing_limit_degrees * DEGREES_TO_RADIANS) * SIMULATION_FREQUENCY;
    std::array<std::uint32_t, 16> words{};
    words[9] = rs::to_bits(swing_velocity);
    words[12] = rs::to_bits(1.0f);
    words[13] = rs::to_bits(1.0f);
    words[14] = 3;
    words[15] = 0;
    return RetailJointParametersRaw{words};
}

void write_quaternion(std::array<std::uint32_t, 20>& words, std::size_t offset, RetailQuaternion value) {
    words[offset] = rs::to_bits(value.x);
    words[offset + 1] = rs::to_bits(value.y);
    words[offset + 2] = rs::to_bits(value.z);
    words[offset + 3] = rs::to_bits(value.w);
}

void write_vector(std::array<std::uint32_t, 20>& words, std::size_t offset, Vector3 value) {
    words[offset] = rs::to_bits(value.x);
    words[offset + 1] = rs::to_bits(value.y);
    words[offset + 2] = rs::to_bits(value.z);
}

RetailJointFramesRaw pack_frames(RetailDriveFrame body_a, RetailDriveFrame body_b,
                                 RetailQuaternion linear_orientation_b) {
    std::array<std::uint32_t, 20> words{};
    write_quaternion(words, 0, body_a.orientation);
    write_vector(words, 4, body_a.translation);
    write_quaternion(words, 8, body_b.orientation);
    write_vector(words, 12, body_b.translation);
    write_quaternion(words, 16, linear_orientation_b);
    return RetailJointFramesRaw{words};
}

RetailJointFramesRaw truck_joint_frames(RetailDriveFrames frames) {
    return pack_frames(frames.body_a, frames.body_b, frames.body_b.orientation);
}

RetailQuaternion wheel_joint_orientation() {
    const auto [sin, cos] = trigonometry::sin_cos(HALF_PI);
    return drive_frames::retail_quaternion_from_basis(
        Basis3{{{{1.0f, 0.0f, 0.0f}, {0.0f, cos, sin}, {0.0f, -sin, cos}}}});
}

RetailJointFramesRaw wheel_joint_frames(RetailQuaternion orientation, float z_offset) {
    return pack_frames(RetailDriveFrame{orientation, Vector3::ZERO},
                       RetailDriveFrame{orientation, Vector3{0.0f, 0.0f, z_offset}}, orientation);
}

constexpr RetailJointRecord joint(BodyId definition_body_0, BodyId definition_body_1,
                                  RetailJointParametersRaw parameters, RetailJointFramesRaw frames) {
    return RetailJointRecord{definition_body_0, definition_body_1, parameters, frames};
}

}  // namespace

std::array<RetailJointRecord, JOINT_COUNT> default_joint_records() {
    return joint_records(RetailJointSettings::STOCK);
}

std::array<RetailJointRecord, JOINT_COUNT> joint_records(RetailJointSettings settings) {
    const auto truck = truck_parameters(settings);
    const auto wheel = wheel_parameters(settings);
    const auto truck_frames = rs::map(drive_frames::default_truck_drive_frames(), truck_joint_frames);
    const auto wheel_orientation = wheel_joint_orientation();
    const float wheel_offset = AuthoredTransformInputs::STOCK.wheel_x_distance;
    const auto positive_wheel_frames = wheel_joint_frames(wheel_orientation, wheel_offset);
    const auto negative_wheel_frames = wheel_joint_frames(wheel_orientation, -wheel_offset);
    return {
        joint(BodyId::Deck, BodyId::FrontTruck, truck, truck_frames[0]),
        joint(BodyId::Deck, BodyId::BackTruck, truck, truck_frames[1]),
        joint(BodyId::FrontTruck, BodyId::RightFrontWheel, wheel, positive_wheel_frames),
        joint(BodyId::FrontTruck, BodyId::LeftFrontWheel, wheel, negative_wheel_frames),
        joint(BodyId::BackTruck, BodyId::RightBackWheel, wheel, positive_wheel_frames),
        joint(BodyId::BackTruck, BodyId::LeftBackWheel, wheel, negative_wheel_frames),
    };
}

}  // namespace s3::physics::joint_records
