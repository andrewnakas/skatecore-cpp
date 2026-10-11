// Port of skate-game/src/physics/handplant/settings.rs
//! TU3 cached layouts55B5940075F78842 and0F38084037CB9175.
#pragma once
#include <array>
#include <cstdint>
#include <string>

#include "s3/core/point_graph.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::handplant::settings {

using ::s3::point_graph::PointGraph;

struct Settings {
    std::array<PointGraph<4>, 7> window{};
    float depth = 0.0f;
    float window_drop = 0.0f;
    PointGraph<8> time_warp{};
    PointGraph<8> out_heading{};
    PointGraph<8> into_rotation{};
    PointGraph<8> out_rotation{};
    PointGraph<4> hand_radius{};
    float curve_half_time = 0.0f;
    float entry_blend = 0.0f;
    float hand_out = 0.0f;
    float hand_into = 0.0f;
    float hand_release = 0.0f;
    float minimum_speed = 0.0f;
    float minimum_slope = 0.0f;
    float rotation_time = 0.0f;
    float committed_time = 0.0f;
    float minimum_out_speed = 0.0f;
    float hand_approach = 0.0f;
    std::int32_t direction_frames = 0;
    float apex_radius = 0.0f;
    float apex_angle = 0.0f;
    std::array<float, 3> animation{};
    float truck_distance = 0.0f;

    static rs::Result<Settings, std::string> load(const sd::Collections& data);
};

}  // namespace s3::game::physics::handplant::settings
