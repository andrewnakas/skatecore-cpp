// Port of skate-core/src/input/turn_remap.rs.
// Complete TU3 SetTurning vector remap, 82BB4248.
// Curves and offsets are supplied from the native animation_turning layout.
#pragma once
#include <array>

#include "s3/core/point_graph.hpp"
#include "s3/core/rs.hpp"

namespace s3::input::turn_remap {

using point_graph::PointGraph;

struct TurnRemap {
    PointGraph<16> magnitude;
    PointGraph<16> angle;
    float angle_offset = 0.0f;

    std::array<float, 4> apply(std::array<float, 2> input) const;
};

}  // namespace s3::input::turn_remap
