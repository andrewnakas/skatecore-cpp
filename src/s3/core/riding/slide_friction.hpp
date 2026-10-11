// Port of skate-core/src/riding/slide_friction.rs.
// Complete TU3 Toolkit_CalcSlideFriction 82D92970; Ground force tag 1.
#pragma once
#include <array>

#include "s3/core/point_graph.hpp"

namespace s3::riding::slide_friction {

using point_graph::PointGraph;

struct SlideFrictionSettings {
    PointGraph<16> angle_response{};  // +196/+260
    PointGraph<8> speed_response{};   // +324/+356
    PointGraph<16> time_response{};   // +388/+452
    float scalar_516 = 0.0f;
    float heading_time_limit = 0.0f;  // surface +236
    float friction = 0.0f;            // surface +240
};

struct SlideFrictionInput {
    float heading_time = 0.0f;
    std::array<float, 4> normal{};     // Processed +464
    std::array<float, 4> velocity{};   // +400
    std::array<float, 4> side_axis{};  // +64
    float surface_speed = 0.0f;        // +2656
    float scalar_2764 = 0.0f;
};

std::array<float, 8> calculate(const SlideFrictionSettings& s, const SlideFrictionInput& i);

}  // namespace s3::riding::slide_friction
