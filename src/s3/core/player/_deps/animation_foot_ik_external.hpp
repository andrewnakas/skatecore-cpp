// TEMP DEP (stage P2): replace with stage A header `s3/core/animation/foot_ik/external.hpp`.
// Only `ExternalTarget` (the item player/** uses) from
// skate-core/src/animation/foot_ik/external.rs; used while the stage-A header is missing.
#pragma once
#if __has_include("s3/core/animation/foot_ik/external.hpp")
#include "s3/core/animation/foot_ik/external.hpp"
#else
#include <array>

#include "s3/core/rs.hpp"

namespace s3::animation::foot_ik::external {

struct ExternalTarget {
    /// SkeletonIK544 /608; setters distinguish the supplied coordinate space.
    std::array<float, 4> world_position{};
    std::array<float, 4> animation_position{};
    /// SkeletonIK736,800,828. The normal is measured against world up.
    std::array<float, 4> normal{};
    bool normal_set = false;
    float normal_blend = 0.0f;
};

}  // namespace s3::animation::foot_ik::external
#endif
