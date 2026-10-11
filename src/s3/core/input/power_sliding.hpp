// Port of skate-core/src/input/power_sliding.rs.
// Complete PowerSliding update82BB28E8 at resolved graph/component boundaries.
// Registration82F89300; factory82BC9060; vtable82320328.
#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <tuple>

#include "s3/core/point_graph.hpp"
#include "s3/core/rs.hpp"

namespace s3::input::power_sliding {

using point_graph::PointGraph;

struct State {
    float elapsed = 0.0f;
    float previous_right = 0.0f;
    float previous_left = 0.0f;
    std::uint32_t flags = 0;
};

struct Settings {
    /// Global slot332 layout1684 and1632.
    float minimum_speed = 0.0f;
    float minimum_slide_time = 0.0f;
    /// Graph4 layouts x1408/y1424, x1360/y1376, x1456/y1472.
    PointGraph<4> stop_time;
    PointGraph<4> speed_response;
    PointGraph<4> angle_response;
};

struct Input {
    /// PhysOut64->0 and PhysOut4->164.
    std::uint32_t category = 0;
    float speed = 0.0f;
    std::optional<float> right_slide;
    std::optional<float> left_slide;
    /// Results of82454FD8 for keys830BE528 and830C0818.
    bool right_query = false;
    bool left_query = false;
    /// MotionGraph interface virtual224,82595930.
    float graph_scalar = 0.0f;
};

/// 82595930 arithmetic.
/// Inputs are PhysOut4->vector80,
/// PhysOut32->basis row32 and PhysOut4->byte273. None means absent Physical.
float alignment(std::optional<std::tuple<std::array<float, 4>, std::array<float, 4>, std::uint8_t>> input);

/// Clock calls preserve the conditional first query and two subsequent timer
/// queries. The native context obtains each through its clock virtual40.
void update(State& state, std::array<std::uint32_t, 5>& latch, Input input, const Settings& settings,
            const std::function<float()>& clock);

}  // namespace s3::input::power_sliding
