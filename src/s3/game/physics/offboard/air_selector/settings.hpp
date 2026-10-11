// Port of skate-game/src/physics/offboard/air_selector/settings.rs
//! Selector-owned stock settings. Missing authored fields are errors.
#pragma once
#include <string>

#include "s3/core/player/offboard/air_selector.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/offboard/settings/curves.hpp"

namespace s3::game::physics::offboard::air_selector::settings {

/// #[path = "../settings/curves.rs"] mod curves;
namespace curves = ::s3::game::physics::offboard::settings::curves;

struct Settings {
    sc::player::offboard::air_selector::Settings query{};
    sc::point_graph::PointGraph<8> blend{};
    ///82C209E4: physics_grinds+636, not measured board geometry.
    float deck_center_to_truck = 0.0f;

    static rs::Result<Settings, std::string> load(const sd::Collections& data);
};

}  // namespace s3::game::physics::offboard::air_selector::settings
