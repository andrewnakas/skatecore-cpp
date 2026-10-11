// Port of skate-core/src/air/trajectory/grind_surface/classify.rs.
#pragma once
#include "s3/core/air/trajectory/grind_surface.hpp"

namespace s3::air::trajectory::grind_surface::classify {

/// 82C20ED4..82C213F4, including the helpers 82C21410/21558/216A0.
GrindSurface resolve(const Investigation& plan, const std::array<std::optional<ProbeHit>, 7>& hits,
                     std::uint32_t previous_flags);

}  // namespace s3::air::trajectory::grind_surface::classify
