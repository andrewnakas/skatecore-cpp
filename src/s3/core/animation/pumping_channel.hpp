// Port of skate-core/src/animation/pumping_channel.rs.
//! Pumping behavior82BAC1B0 and instance seeds82BAC528.
#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <utility>

#include "s3/core/point_graph.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::pumping_channel {

using point_graph::PointGraph;

struct Settings {
    PointGraph<8> amplify;
    float input_blend = 0.0f;
    float maximum_physics_pump = 0.0f;
    float new_pump_threshold = 0.0f;
    float blend_in = 0.0f;
    float blend_out = 0.0f;
};

struct Update {
    std::optional<std::size_t> start;
    std::optional<std::pair<std::size_t, float>> influence;
};

/// Manual `impl Default` (all zero / None): `State{}` == `State::default_()`.
struct State {
    float value = 0.0f;
    float filtered = 0.0f;
    std::optional<std::size_t> channel;
    float peak = 0.0f;

    static State default_() { return State{}; }

    Update update(float physics_pump, std::array<bool, 5> occupied, const Settings& s) {
        const float previous = value;
        float input = physics_pump / s.maximum_physics_pump;
        const float lower = -input >= 0.0f ? 0.0f : input;
        input = 1.0f - lower >= 0.0f ? lower : 1.0f;
        //82BAC2F8 is fmadds; preserve the single final rounding.
        filtered = rs::mul_add(input, s.input_blend, (1.0f - s.input_blend) * filtered);
        value = s.amplify.evaluate(filtered);
        std::optional<std::size_t> start;
        if (!(previous > s.new_pump_threshold) && value > s.new_pump_threshold) {
            for (std::size_t i = 0; i < occupied.size(); ++i) {
                if (!occupied[i]) {
                    start = i;
                    break;
                }
            }
        }
        if (start) {
            channel = *start;
            peak = value;
        }
        std::optional<std::pair<std::size_t, float>> influence;
        if (value >= previous && value >= peak) {
            if (channel) {
                peak = value;
                influence = std::make_pair(*channel, value);
            }
        }
        return Update{start, influence};
    }
};

}  // namespace s3::animation::pumping_channel
