// Port of skate-core/src/input/turn_conditioner.rs.
// Complete steering conditioner 82DEFF38, called by82DEFB30.
// The settings decoder supplies global830CFDA4 slot204's verified layout.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/point_graph.hpp"
#include "s3/core/rs.hpp"

namespace s3::input::turn_conditioner {

using point_graph::PointGraph;

struct Settings {
    /// Layout688/704/720, corresponding to conditioner216/180/144.
    std::array<std::array<float, 4>, 3> filter_coefficients{};
    /// Layout x304/y336, x240/y272, x32/y64, x384/y400.
    PointGraph<8> input_curve;
    PointGraph<8> quickness_curve;
    PointGraph<8> speed_curve;
    PointGraph<4> smoothing_curve;
    /// Layout768,868,872,896,900,904,908,912,924,928,932,936,940.
    std::array<float, 13> parameters{};
};

struct State {
    /// Conditioner112..140, in native order.
    std::array<float, 8> history{};
    /// Complete9-word filters at216,180,144; first4 words refresh every call.
    std::array<std::array<float, 9>, 3> filters{};

    /// Steering-history portion of native reset82DEFAA0. Cached coefficients
    /// survive reset; the update refreshes them from the settings layout.
    void reset_history() {
        history.fill(0.0f);
        for (auto& filter : filters) {
            for (std::size_t i = 4; i < 9; ++i) filter[i] = 0.0f;
        }
    }
};

struct Input {
    float body_160 = 0.0f;
    float body_176 = 0.0f;
    float bundle_36_field_160 = 0.0f;
    float bundle_32_field_264 = 0.0f;
    std::uint8_t animation_152 = 0;
    std::uint8_t animation_156 = 0;
};

/// PhysOut_Animation32/36/40/44/48/52/56/60, in native memory order.
using Output = std::array<float, 8>;

Output update(State& state, Input input, const Settings& settings);

}  // namespace s3::input::turn_conditioner
