// Port of skate-core/src/physics/world_contact/feature.rs.
#pragma once
#include "s3/core/physics/world_contact/arithmetic.hpp"

namespace s3::physics::world_contact::feature {

/// Complete 82AC6E50 segment initialization. Writes origin, normalized direction
/// and broadcast length; preserves the unwritten plane vector at words8..12.
/// (`output` points at a 16-word segment.)
void initialize_feature_segment(std::uint32_t* output, std::array<std::uint32_t, 4> origin,
                                std::array<std::uint32_t, 4> end);
inline void initialize_feature_segment(std::array<std::uint32_t, 16>& output,
                                       std::array<std::uint32_t, 4> origin,
                                       std::array<std::uint32_t, 4> end) {
    initialize_feature_segment(output.data(), origin, end);
}

/// Complete capsule maximum-feature callback82AD98A0. `segment_scratch` carries
/// the native local64-byte segment, including its unwritten plane vector.
/// No fabricated value is assigned to those copied scratch words. Native r4
/// mode is unused by this capsule callback.
void capsule_maximum_feature(const std::array<std::uint32_t, 48>& gp, std::array<std::uint32_t, 4> direction,
                             MaximumFeature& output, std::array<std::uint32_t, 16>& segment_scratch);

}  // namespace s3::physics::world_contact::feature
