// Port of skate-core/src/physics/construction/authored_transforms.rs
// (Rust module path physics::drive_frames::authored, via #[path]).
//
// Authored seven-part matrices passed to Part::SetTransform by TU3 82C0ADF0.
// This function is called by construction/reset before world-pose publication.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/physics/drive_frames.hpp"

namespace s3::physics::drive_frames::authored {

/// Resolved stock attributes read by InitializeTransforms. Attribute lookup
/// failure selects the engine's runtime fallback scalar at830D0850; callers
/// resolving absent attributes must supply that actual initialized value.
struct AuthoredTransformInputs {
    /// physicsdeck.DeckMidLength; full attribute key C94D65091DCB828B.
    float deck_mid_length = 0.0f;
    /// physicswheels.WheelXDist; full attribute key 5A84C435DC340AF8.
    float wheel_x_distance = 0.0f;
    /// physicstrucks layout+4, +8, +16.
    float truck_z_position_front = 0.0f;
    float truck_z_position_back = 0.0f;
    float truck_y_position = 0.0f;

    /// Actual stock XML scalar words, not constructor output snapshots.
    static const AuthoredTransformInputs STOCK;
};
inline constexpr AuthoredTransformInputs AuthoredTransformInputs::STOCK = AuthoredTransformInputs{
    f32_from_bits(0x3f170a3du), f32_from_bits(0x3dc28f5cu), f32_from_bits(0xbd54fdf4u),
    f32_from_bits(0xbd54fdf4u), f32_from_bits(0xbd676c8bu),
};
S3_DERIVE_EQ(AuthoredTransformInputs, deck_mid_length, wheel_x_distance, truck_z_position_front,
             truck_z_position_back, truck_y_position)

/// Complete authored matrix calculation, in body order wheels0..3, trucks4..5,
/// deck6. Native publication order is deck6 first, then bodies0..5. Front truck
/// is identity; back truck uses the actual SinCos(pi/2) result about Y. The
/// tiny polynomial cosine residual is retained, never replaced by exact zero.
std::array<RetailAffineTransform, 7> authored_body_transforms(AuthoredTransformInputs input);

/// Native affine rows, ready for reset_skateboard's `authored` argument. All
/// fourth lanes are zero, including the affine translation:82C0ADF0 explicitly
/// clears them. This does not perform the downstream COM/quaternion conversion.
std::array<std::array<std::uint32_t, 16>, 7> authored_body_pose_records(AuthoredTransformInputs input);

}  // namespace s3::physics::drive_frames::authored

// `pub use authored::{...}` re-exports (kept in the child header so either include order works).
namespace s3::physics::drive_frames {
using authored::authored_body_pose_records;
using authored::authored_body_transforms;
using authored::AuthoredTransformInputs;
}  // namespace s3::physics::drive_frames
