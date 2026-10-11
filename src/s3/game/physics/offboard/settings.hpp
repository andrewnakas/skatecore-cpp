// Port of skate-game/src/physics/offboard/settings.rs
//! Stock Biped settings82D7B630 and metric queries82D7AFD8/82D16680.
#pragma once
#include <array>
#include <optional>
#include <string>
#include <tuple>

#include "s3/core/player/offboard/air_launch.hpp"
#include "s3/core/player/offboard/controller.hpp"
#include "s3/core/player/offboard/ground_sync.hpp"
#include "s3/core/player/offboard/movement_intent.hpp"
#include "s3/core/player/offboard/movement_velocity.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/data/animation_metadata.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/offboard/settings/board.hpp"
#include "s3/game/physics/offboard/settings/curves.hpp"
#include "s3/game/physics/offboard/settings/metrics.hpp"

namespace s3::game::physics::offboard::settings {

using sc::point_graph::PointGraph;

struct Settings {
    sc::player::offboard::controller::Settings controller{};
    sc::player::offboard::ground_sync::BoardSettings board{};
    std::array<std::optional<sc::player::offboard::controller::ClipMetric>, 3> metrics{};
    ///82D310F8 full key DF759B46440F16E9.
    PointGraph<8> movement_vs_stick_angle{};
    ///82D310F8 full key2DD95B399BAE313E.
    PointGraph<8> turn_vs_stick_angle{};
    sc::player::offboard::air_launch::Settings air_launch{};

    /// Share the same native ABIN metadata loaded for SkaterAnimation. Required
    /// stock fields/banks/clips fail loading; an absent matching attribute is None.
    static rs::Result<Settings, std::string> load(const sd::Collections& data, const sd::AnimationMetadata& metadata);

    std::tuple<sc::player::offboard::controller::Settings,
               std::array<std::optional<sc::player::offboard::controller::ClipMetric>, 3>, PointGraph<8>,
               PointGraph<8>, sc::player::offboard::air_launch::Settings>
    into_controller_parts() &&;
};

}  // namespace s3::game::physics::offboard::settings
