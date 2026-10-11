// Port of skate-core/src/physics/joint_records.rs.
//
// TU3 skateboard joint definitions reconstructed from stock collection data.
//
// `SkateboardBody::CreateJoints` (`0x82C0C268`) builds two deck/truck joints
// followed by four truck/wheel joints. This module emits the native parameter
// and frame payloads that `Assembly::Initialize` consumes; it does not replace
// them with a third-party constraint type.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "s3/core/physics/board.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/physics/f32_bits.hpp"

namespace s3::physics::joint_records {

using board::BodyId;

namespace tu3 {
inline constexpr std::uint32_t CREATE_JOINTS = 0x82C0C268u;
inline constexpr std::uint32_t ASSEMBLY_INITIALIZE = 0x82ADFAF8u;
inline constexpr std::uint32_t ASSEMBLY_POPULATE = 0x82ADFBB8u;
inline constexpr std::uint32_t FRAME_PACK_0 = 0x82C00940u;
inline constexpr std::uint32_t FRAME_PACK_1 = 0x82C00AF0u;
inline constexpr std::uint32_t FRAME_PACK_2 = 0x82BD3858u;
}  // namespace tu3

inline constexpr std::size_t JOINT_COUNT = 6;

struct RetailJointSettings {
    float truck_twist_limit_degrees = 0.0f;
    float truck_twist_angle_degrees = 0.0f;
    float wheel_swing_limit_degrees = 0.0f;

    /// Values decoded from the stock `physicstrucks` and `physicswheels`
    /// collection XMLs read by `CreateJoints`.
    static const RetailJointSettings STOCK;
};
inline constexpr RetailJointSettings RetailJointSettings::STOCK = RetailJointSettings{
    f32_from_bits(0x42AC3333u),
    f32_from_bits(0x41600000u),
    f32_from_bits(0x49742400u),
};
S3_DERIVE_EQ(RetailJointSettings, truck_twist_limit_degrees, truck_twist_angle_degrees,
             wheel_swing_limit_degrees)

struct RetailJointParametersRaw {
    std::array<std::uint32_t, 16> words{};
};
S3_DERIVE_EQ(RetailJointParametersRaw, words)

struct RetailJointFramesRaw {
    std::array<std::uint32_t, 20> words{};
};
S3_DERIVE_EQ(RetailJointFramesRaw, words)

struct RetailJointRecord {
    BodyId definition_body_0 = BodyId::RightFrontWheel;
    BodyId definition_body_1 = BodyId::RightFrontWheel;
    RetailJointParametersRaw parameters;
    RetailJointFramesRaw frames;

    /// Body written to live `Joint+0x10` by assembly activation.
    constexpr BodyId live_body_a() const { return definition_body_1; }

    /// Body written to live `Joint+0x14` by assembly activation.
    constexpr BodyId live_body_b() const { return definition_body_0; }
};
S3_DERIVE_EQ(RetailJointRecord, definition_body_0, definition_body_1, parameters, frames)

/// Complete stock record set in the unrolled write order of TU3
/// `SkateboardBody::CreateJoints`.
std::array<RetailJointRecord, JOINT_COUNT> default_joint_records();

std::array<RetailJointRecord, JOINT_COUNT> joint_records(RetailJointSettings settings);

}  // namespace s3::physics::joint_records
