// Port of skate-core/src/physics/skeleton_body/mod.rs.
// Physical skater assembly and its postphysics observations.
// The body has 26 parts; animation and COM weighting use its first 24.
//
// The Rust submodules are private `mod`s re-exported with `pub use`; in C++ each
// child header re-exports its public items into `s3::physics::skeleton_body`.
// Include this header (not the children) as the entry point.
#pragma once
#include <cstddef>

namespace s3::physics::skeleton_body {

inline constexpr std::size_t PART_COUNT = 26;
inline constexpr std::size_t ANIMATION_PART_COUNT = 24;
inline constexpr std::size_t JOINT_COUNT = 22;

// Forward declarations for the mutually dependent submodules.
namespace runtime {
class SkeletonBody;
}
namespace collision_feedback {
struct SkeletonCollisionFeedback;
}
using collision_feedback::SkeletonCollisionFeedback;
using runtime::SkeletonBody;

}  // namespace s3::physics::skeleton_body

#include "s3/core/physics/skeleton_body/record.hpp"
#include "s3/core/physics/skeleton_body/construction.hpp"
#include "s3/core/physics/skeleton_body/collision_mode.hpp"
#include "s3/core/physics/skeleton_body/collision_feedback.hpp"
#include "s3/core/physics/skeleton_body/collision_vector.hpp"
#include "s3/core/physics/skeleton_body/drive_dynamics.hpp"
#include "s3/core/physics/skeleton_body/drive_frames.hpp"
#include "s3/core/physics/skeleton_body/target_update.hpp"
#include "s3/core/physics/skeleton_body/targets.hpp"
#include "s3/core/physics/skeleton_body/runtime.hpp"
#include "s3/core/physics/skeleton_body/drives.hpp"
#include "s3/core/physics/skeleton_body/joints.hpp"
#include "s3/core/physics/skeleton_body/errors.hpp"
