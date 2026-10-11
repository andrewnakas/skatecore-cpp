// Port of skate-core/src/physics/truck_frames.rs.
// Dynamic truck frames, TU3 `SkateboardBody::SetTruckDriveFrames` (0x82C0B9C0).
#pragma once
#include <array>

#include "s3/core/physics/drive_frames.hpp"

namespace s3::physics::truck_frames {

using drive_frames::RetailAffineTransform;
using drive_frames::RetailDriveFrames;

/// `base` is CalculateTruckTransforms output in native +7712/+7776 order.
/// Targets are +7680/+7684. Frame output follows +6768/+6832 drive order.
std::array<RetailDriveFrames, 2> steering_drive_frames(std::array<RetailAffineTransform, 2> base,
                                                       std::array<float, 2> targets);

///82C0B9C0 caches front7840/back7904 for both physical drives and the
///physical pose worker82DB6698. Preserve this separately from the base frames.
std::array<RetailAffineTransform, 2> steering_truck_transforms(std::array<RetailAffineTransform, 2> base,
                                                               std::array<float, 2> targets);

}  // namespace s3::physics::truck_frames
