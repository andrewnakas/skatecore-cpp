// Port of skate-core/src/riding/pumping.rs.
// Pumping lifecycle, native geometry and CalcPumpForce82D933D0 publication.
// Ground supplies the current stock mode and queues the resulting board force.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/riding/pumping/controller.hpp"
#include "s3/core/riding/pumping/geometry.hpp"
#include "s3/core/riding/pumping/settings.hpp"
#include "s3/core/riding/pumping/state.hpp"

namespace s3::riding::pumping {

struct PumpForceInput {
    std::uint32_t flags_2476 = 0;
    float mode_multiplier = 0.0f;  // selected-mode layout+8
    float pumping_scalar = 0.0f;   // Pumping+48 argument f1
    float input_scalar_2660 = 0.0f;
    float timestep = 0.0f;  // Processed+2604
    std::array<float, 4> direction_432{};
    std::array<float, 4> normal_threshold{};
};

std::array<float, 8> calculate(const PumpForceInput& i);

}  // namespace s3::riding::pumping
