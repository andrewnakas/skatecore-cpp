// Port of skate-core/src/physics/contact_feedback/collision_info.rs.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/physics/contact_feedback.hpp"

namespace s3::physics::contact_feedback::collision_info {

/// Complete 82C00CA0 selective per-frame reset. Wheel surface/audio/object
/// slots and line-test state survive; this differs from startup 82C00D68.
void reset_frame_collision_info(CollisionInfo& info);

/// Complete 82C08818 surface vote. Noncontacting wheels still contribute one
/// vote; contacting wheels contribute four. Equal counts favor the lower ID.
/// IDs14/15 are accumulated but not considered by the native winner scan.
void choose_physics_surface(CollisionInfo& info);

/// Typed82C08818 boundary. Values are the retained wheel surface outputs;
/// query misses and contact replacements belong to their original producers.
std::uint32_t choose_surface(std::array<std::uint32_t, 4> surfaces, std::array<bool, 4> contacts,
                             bool forced_twelve);

}  // namespace s3::physics::contact_feedback::collision_info

namespace s3::physics::contact_feedback {
using collision_info::choose_physics_surface;
using collision_info::choose_surface;
using collision_info::reset_frame_collision_info;
}  // namespace s3::physics::contact_feedback
