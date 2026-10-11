// Port of skate-core/src/riding/straighten.rs.
// Complete TU3 Toolkit_CalcStraightenOutTorque 82D92B60.
#pragma once
#include <array>

#include "s3/core/point_graph.hpp"

namespace s3::riding::straighten {

using point_graph::PointGraph;

struct StraightenSettings {
    PointGraph<16> time_response{};    // settings +768/+832
    float opposite_turn_limit = 0.0f;  // +896
    float time_scalar = 0.0f;          // +900
    float heading_time_limit = 0.0f;   // surface +236
    float strength = 0.0f;             // surface +232
};

struct StraightenInput {
    float heading_time = 0.0f;
    float scalar_2764 = 0.0f;
    float turn = 0.0f;                // +2676
    std::array<float, 4> forward{};   // +352
    std::array<float, 4> velocity{};  // +400
    std::array<float, 4> normal{};    // +464
};

std::array<float, 4> calculate(const StraightenSettings& s, const StraightenInput& i);

}  // namespace s3::riding::straighten
