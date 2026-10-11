// Port of skate-game/src/physics/air_reckoning/settings.rs.
//! Stock classes are verified against original8289D180 and828A0054..00A0:
//! physics_reckoning7CE1B6B581709C6F, physics_bodyspin05EC9CB03A5B2EEE.
#pragma once
#include <array>
#include <string>
#include <utility>

#include "s3/core/air/body_flip.hpp"
#include "s3/core/air/body_spin.hpp"
#include "s3/core/air/reckoning.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::air_reckoning::settings {

struct Mode {
    bool easy_body_spins = false;
    bool perfect_body_flips = false;
};

rs::Result<std::pair<::s3::air::reckoning::Settings, std::array<Mode, 5>>, std::string> load(
    const sd::Collections& data);

// Private helpers in Rust.
rs::Result<::s3::point_graph::PointGraph<8>, std::string> negative_graph(const sd::Collections& data,
                                                                        const std::string& class_,
                                                                        const std::string& name);
rs::Result<::s3::point_graph::PointGraph<8>, std::string> graph(const sd::Collections& data, const std::string& name);

}  // namespace s3::game::physics::air_reckoning::settings
