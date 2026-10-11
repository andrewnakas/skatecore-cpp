// Port of skate-core/src/riding/heading.rs.
// Complete TU3 Toolkit_CalcHeadingTorque 82D92CF8. Ground calls this before
// AntiFlipTorque; the two are distinct. The caller owns board+284 history.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/point_graph.hpp"

namespace s3::riding::heading {

using point_graph::PointGraph;

struct HeadingSettings {
    float manual_wrong_wheel_scalar = 0.0f;  // +904
    float manual_damping = 0.0f;             // +908
    float speed_max = 0.0f;                  // +912
    float heading_strength = 0.0f;           // +916
    float turn_strength = 0.0f;              // +920
    PointGraph<8> angular_response{};        // +940/+972
    PointGraph<8> manual_response{};         // +1020/+1052
    PointGraph<8> inclination_response{};    // +1084/+1116
    PointGraph<8> speed_response{};          // +1148/+1180
    /// Live four-lane threshold at 830BD350, initialized by 82F826F8.
    std::array<float, 4> normal_threshold{};
};

struct HeadingInput {
    float balance = 0.0f;      // Processed +2720
    float manual_turn = 0.0f;  // +2672
    std::uint32_t flags_2472 = 0;
    float timestep = 0.0f;            // +2604
    float signed_speed = 0.0f;        // +2612
    float manual_curve_input = 0.0f;  // +2652
    float turn_2712 = 0.0f;
    float scalar_2740 = 0.0f;
    std::array<float, 4> velocity{};          // +432
    std::array<float, 4> normal{};            // +464
    std::array<float, 4> angular_velocity{};  // +720
    /// Ground caller's r6 matrix, in native column order. No host orthogonalization.
    std::array<std::array<float, 4>, 4> transform{};
};

std::array<float, 4> calculate(const HeadingSettings& settings, const HeadingInput& input, float& previous);

}  // namespace s3::riding::heading
