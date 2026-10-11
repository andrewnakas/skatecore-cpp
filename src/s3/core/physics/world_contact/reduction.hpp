// Port of skate-core/src/physics/world_contact/reduction.rs.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "s3/core/physics/world_contact/arithmetic.hpp"

namespace s3::physics::world_contact::reduction {

using arithmetic::V;

/// Complete geometric predicate 8277A208. Body IDs are checked by its caller.
/// The native comparison uses the *smaller* absolute plane displacement.
bool coplanar_contacts(const ContactRecord& a, const ContactRecord& b);

/// Full 827798F8 point selection. Inputs are paired world-space positions;
/// output slots beyond the returned count retain the native write behavior.
/// The caller provides at least one pair, as the native function requires.
std::size_t select_contact_points(const std::vector<V>& a, const std::vector<V>& b, V normal,
                                  std::array<std::uint32_t, 4>& output);

/// Complete 82779EB0 grouping/reduction. Retained contacts are restored to
/// original order; discarded tail records are not cleared. No drop count change.
void reduce(std::array<ContactRecord, 50>& records, std::uint32_t& count);

}  // namespace s3::physics::world_contact::reduction
