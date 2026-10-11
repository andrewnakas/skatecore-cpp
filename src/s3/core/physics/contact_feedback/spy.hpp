// Port of skate-core/src/physics/contact_feedback/spy.rs.
#pragma once
#include <array>
#include <cstdint>
#include <functional>

#include "s3/core/physics/slice.hpp"

namespace s3::physics::contact_feedback::spy {

/// Complete TU3 82AE1608. Compacts eligible256-byte compiled contact rows into
/// 112-byte spies at the same buffer start. `spy_count` increments from its input
/// value; it does not choose the write offset and is not reset by this routine.
/// `body_center` resolves the exact body pointer to its packed COM/ID vector.
void spy_contact_jacobians(Slice<std::uint32_t> storage, std::uint32_t contact_count, std::uint32_t& spy_count,
                           float frequency,
                           const std::function<std::array<std::uint32_t, 4>(std::uint32_t)>& body_center);

}  // namespace s3::physics::contact_feedback::spy

namespace s3::physics::contact_feedback {
using spy::spy_contact_jacobians;
}  // namespace s3::physics::contact_feedback
