// Port of skate-core/src/riding/ground_force.rs.
// Complete TU3 82D931E8, called twice by Ground Update for tags 4 and 5.
// The research index has not recovered its original name. Keep its four
// caller-supplied scalar arguments explicit instead of inventing units.
#pragma once
#include <array>

#include "s3/core/rs.hpp"

namespace s3::riding::ground_force {

struct GroundForceSettings {
    float range_1220 = 0.0f;
    float speed_scale_1224 = 0.0f;
    float scale_1228 = 0.0f;
    std::array<float, 4> normal_threshold{};
};

struct GroundForceInput {
    float argument_1 = 0.0f;
    float application_z = 0.0f;
    float argument_3 = 0.0f;
    float argument_4 = 0.0f;
    float balance = 0.0f;        // Processed +2720
    float surface_speed = 0.0f;  // +2656
    std::array<float, 4> axis_384{};
    std::array<float, 4> velocity_400{};
    std::array<float, 4> axis_544{};
};

std::array<float, 8> calculate(const GroundForceSettings& s, const GroundForceInput& i);

}  // namespace s3::riding::ground_force
