// Port of skate-core/src/physics/hook_drive/frames.rs.
// Full 82C00940/82C00AF0 matrix33 conversion. Unlike Part::SetTransform,
// these functions do not normalize the selected quaternion afterward.
#pragma once
#include <array>
#include <cstdint>

namespace s3::physics::hook_drive::frames {

/// pub(super)
float rsqrt(float v);
/// 82C00940 preserves both translations and the parent quaternion.
void set_child_angular_frame(std::array<std::uint32_t, 16>& frames, std::array<std::uint32_t, 12> basis);
/// 82C00AF0 preserves both translations and the child quaternion.
void set_parent_angular_frame(std::array<std::uint32_t, 16>& frames, std::array<std::uint32_t, 12> basis);

}  // namespace s3::physics::hook_drive::frames

// `pub use frames::{set_child_angular_frame, set_parent_angular_frame}`.
namespace s3::physics::hook_drive {
using frames::set_child_angular_frame;
using frames::set_parent_angular_frame;
}  // namespace s3::physics::hook_drive
