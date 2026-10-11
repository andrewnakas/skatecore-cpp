// Port of skate-core/src/physics/contact_feedback/mod.rs.
// TU3 post-solver contact observations and their native record lifetimes.
#pragma once
#include <array>
#include <cstdint>

namespace s3::physics::contact_feedback {

/// SkateboardBody+64..+876, retained as guest big-endian bytes.
using CollisionInfo = std::array<std::uint8_t, 812>;

}  // namespace s3::physics::contact_feedback

// Submodules; each child header carries its own `pub use` re-exports.
#include "s3/core/physics/contact_feedback/board_reports.hpp"
#include "s3/core/physics/contact_feedback/collision_info.hpp"
#include "s3/core/physics/contact_feedback/compression.hpp"
#include "s3/core/physics/contact_feedback/reports.hpp"
#include "s3/core/physics/contact_feedback/spy.hpp"

namespace s3::physics::contact_feedback {
using board_reports::BoardContactReport;
}  // namespace s3::physics::contact_feedback
