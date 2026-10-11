// Port of skate-core/src/riding/anti_flip.rs.
// Complete TU3 Toolkit_AntiFlipTorque 82D94190, distinct from heading/manual.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/point_graph.hpp"

namespace s3::riding::anti_flip {

using point_graph::PointGraph;

struct AntiFlipSettings {
    PointGraph<8> axis_96_response{};      // +1832/+1864
    PointGraph<8> axis_64_response{};      // +1912/+1944
    std::array<float, 4> normal_threshold{};  // live 830BD350
};

struct AntiFlipInput {
    std::uint32_t flags_2468 = 0;
    float balance = 0.0f;  // +2720
    std::array<float, 4> axis_96{};
    std::array<float, 4> axis_64{};
    std::array<float, 4> projection_axis_544{};
};

std::array<float, 4> calculate(const AntiFlipSettings& s, const AntiFlipInput& i);

}  // namespace s3::riding::anti_flip
