// Port of skate-game/src/physics/player_input/grind/settings.rs.
#pragma once
#include <string>
#include <string_view>

#include "s3/core/point_graph.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::player_input::grind::settings {

using sc::point_graph::PointGraph;

/// `pub(super)`.
struct Settings {
    PointGraph<4> friction{};
    PointGraph<4> slope_threshold{};
    PointGraph<4> vertical_help{};
    PointGraph<4> gravity_vertical{};
    PointGraph<4> gravity_linear{};
    PointGraph<8> exit_lean{};
    float truck_to_wheel = 0.0f;
    float deck_to_truck = 0.0f;
    float test_above = 0.0f;
    float test_below = 0.0f;
    float max_impact = 0.0f;

    static rs::Result<Settings, std::string> load(const sd::Collections& data);
};

/// Private in Rust.
rs::Result<PointGraph<4>, std::string> graph4(const sd::Collections& data, std::string_view name);

}  // namespace s3::game::physics::player_input::grind::settings
