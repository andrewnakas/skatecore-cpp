// Port of skate-core/src/physics/drive_frames.rs.
//
// Scalar port of Skate 3 TU3's skateboard drive-frame construction.
//
// `SkateboardBody::SetDriveFrames2` (`0x82C0C088`) writes an identity matrix
// to the child/second-body frame and writes the computed relative matrix to
// the parent/first-body frame. The retail matrices are affine column-basis
// matrices (`Ri`, `Up`, `At`, translation). This module keeps that convention
// explicit and does not substitute a Bevy joint.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/math.hpp"
#include "s3/core/physics/board.hpp"
#include "s3/core/physics/drive_parameters.hpp"
#include "s3/core/physics/rigid_body.hpp"

namespace s3::physics::drive_frames {

using math::Basis3;
using math::Vector3;
using rigid_body::RetailQuaternion;
using namespace board;

namespace tu3 {
inline constexpr std::uint32_t CALCULATE_TRUCK_TRANSFORMS = 0x82C0BC90u;
inline constexpr std::uint32_t SET_DRIVE_FRAMES_2 = 0x82C0C088u;
inline constexpr std::uint32_t MATRIX_TO_FRAME_A = 0x82BD3A10u;
inline constexpr std::uint32_t MATRIX_TO_FRAME_B = 0x82BD3BD0u;
inline constexpr std::uint32_t PART_SET_TRANSFORM = 0x82BD4318u;
}  // namespace tu3

struct RetailAffineTransform {
    /// Column vectors in retail `Ri`, `Up`, `At` order.
    Basis3 basis;
    Vector3 translation;

    static const RetailAffineTransform IDENTITY;
};
inline constexpr RetailAffineTransform RetailAffineTransform::IDENTITY = RetailAffineTransform{
    Basis3{{{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}}},
    Vector3{0.0f, 0.0f, 0.0f},
};
S3_DERIVE_EQ(RetailAffineTransform, basis, translation)

struct RetailDriveFrame {
    RetailQuaternion orientation;
    Vector3 translation;
};
S3_DERIVE_EQ(RetailDriveFrame, orientation, translation)

struct RetailDriveFrames {
    /// Native child/second-body record written by `SetChildFrame` at +0/+16.
    RetailDriveFrame body_a;
    /// Native parent/first-body record written by `SetParentFrame` at +32/+48.
    RetailDriveFrame body_b;
};
S3_DERIVE_EQ(RetailDriveFrames, body_a, body_b)

/// Inputs read by TU3 `SkateboardBody::CalculateTruckTransforms`.
///
/// The two longitudinal fields retain the decoded retail attribute names.
struct RetailTruckTransformInputs {
    float deck_mid_length = 0.0f;
    float truck_z_position_front = 0.0f;
    float truck_z_position_back = 0.0f;
    float truck_y_position = 0.0f;
    float truck_rotation_axis_angle_degrees = 0.0f;
};
S3_DERIVE_EQ(RetailTruckTransformInputs, deck_mid_length, truck_z_position_front,
             truck_z_position_back, truck_y_position, truck_rotation_axis_angle_degrees)

inline constexpr RetailTruckTransformInputs RETAIL_DEFAULT_TRUCK_TRANSFORM_INPUTS =
    RetailTruckTransformInputs{
        RETAIL_DECK_MID_LENGTH,
        RETAIL_TRUCK_Z_POSITION_FRONT,
        RETAIL_TRUCK_Z_POSITION_BACK,
        RETAIL_TRUCK_Y_POSITION,
        RETAIL_TRUCK_ROTATION_AXIS_ANGLE_DEGREES,
    };

}  // namespace s3::physics::drive_frames

#include "s3/core/physics/construction/authored_transforms.hpp"

namespace s3::physics::drive_frames {

using authored::authored_body_pose_records;
using authored::authored_body_transforms;
using authored::AuthoredTransformInputs;

/// TU3 `SkateboardBody::CalculateTruckTransforms` (`0x82C0BC90`) evaluated
/// from the stock skater-collection inputs.
///
/// The returned order is the native `+7712`, `+7776` storage order. The names
/// of the front/back Z inputs are retained even though the native slots do not
/// follow an intuitive front/back naming order.
std::array<RetailAffineTransform, 2> default_truck_transforms();

/// Authored part matrices from `82C0ADF0`, evaluated from stock collection inputs.
/// These are the inputs to Part::SetTransform, not settled or live COM poses.
std::array<RetailAffineTransform, BODY_COUNT> default_body_transforms();

/// Live center-of-mass transforms emitted by TU3 `Part::SetTransform`
/// (`0x82BD4318`) from the seven authored part transforms.
std::array<RetailAffineTransform, BODY_COUNT> default_live_body_transforms();

/// Quaternions written beside the live center-of-mass transforms by
/// `Part::SetTransform`.
std::array<RetailQuaternion, BODY_COUNT> default_live_body_orientations();

/// Deck-center height that places the initialized wheel spheres exactly on a
/// level Y=0 riding surface.
///
/// This is a geometry-derived fixture placement, not a tuned suspension or
/// contact constant.
inline constexpr float RETAIL_LEVEL_GROUND_DECK_CENTER_HEIGHT =
    RETAIL_WHEEL_RADIUS - RETAIL_TRUCK_Y_POSITION;

/// Zero-steering truck/deck frames in native drive order. TU3 consumes the
/// `+7776` transform before the `+7712` transform.
std::array<RetailDriveFrames, 2> default_truck_drive_frames();

/// Wheel/truck drive frames emitted by the four `CreateWheelDrives` calls.
std::array<RetailDriveFrames, 4> default_wheel_drive_frames();

/// Matrix stage of `SkateboardBody::SetDriveFrames2`.
///
/// The first input is the parent/first body's world transform and the second
/// input is the child/second body's world transform. TU3 writes:
///
/// - identity to the child frame through `SetChildFrame` (`0x82BD3A10`);
/// - `transpose(parent.basis) * child.basis` and the corresponding relative
///   translation to the parent frame through `SetParentFrame` (`0x82BD3BD0`).
RetailDriveFrames set_drive_frames_2(RetailAffineTransform parent_world,
                                     RetailAffineTransform child_world);

/// Dominant-component matrix-to-quaternion calculation in TU3 child/parent
/// frame setters `0x82BD3A10` and `0x82BD3BD0`.
///
/// The function evaluates the same four component candidates as the vector
/// implementation, selects trace/X/Y/Z with its strict comparisons, and uses
/// the accepted Xenon reciprocal-square-root estimate with two recovered fused
/// refinements. Matrix storage remains column-major `Ri`, `Up`, `At`.
RetailQuaternion retail_quaternion_from_basis(Basis3 basis);

/// `impl From<RetailDriveFrames> for RetailDriveFramesRaw`.
drive_parameters::RetailDriveFramesRaw to_raw(RetailDriveFrames frames);

std::array<RetailAffineTransform, 2> calculate_truck_transforms(RetailTruckTransformInputs inputs);

// Module-private in Rust; visible here for the module's #[cfg(test)] tests.
RetailAffineTransform relative_parent_transform(RetailAffineTransform parent_world,
                                                RetailAffineTransform child_world);
Vector3 transpose_relative_translation(Basis3 parent_basis, Vector3 parent_translation,
                                       Vector3 child_translation);

}  // namespace s3::physics::drive_frames
