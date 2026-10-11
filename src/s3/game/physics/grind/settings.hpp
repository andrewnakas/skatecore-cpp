// Port of skate-game/src/physics/grind/settings.rs.
//! Stock runtime data. No map/query settings or manager defaults live here.
#pragma once
#include <string>

#include "s3/core/physics/grind_forces/post.hpp"
#include "s3/core/physics/grind_forces/reckoning.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/grind/substate/settings.hpp"

namespace s3::game::physics::grind::settings {

struct Settings {
    float standard_angular_drag = 0.0f;
    ::s3::point_graph::PointGraph<4> pin_vs_slope{};
    float look_ahead = 0.0f;
    ::s3::point_graph::PointGraph<4> exit_assist{};
    ::s3::physics::grind_forces::post::Settings post{};
    ::s3::physics::grind_forces::reckoning::Settings reckoning{};
    substate::settings::Settings substate{};

    static rs::Result<Settings, std::string> load(const sd::Collections& data);
};

}  // namespace s3::game::physics::grind::settings
