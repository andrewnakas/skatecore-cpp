// Port of skate-game/src/physics/slide_state/settings.rs.
//! Direct original bindings82D94EC8..4F48,82D3AB30/AD3C/AE68 and stock schema.
//! (`impl SlideState::load` is declared in slide_state.hpp.)
#pragma once
#include <string>
#include <string_view>

#include "s3/core/point_graph.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::slide_state::settings {

/// Private in Rust.
rs::Result<sc::point_graph::PointGraph<8>, std::string> curve(const sd::Collections& d, std::string_view c,
                                                              std::string_view k, std::string_view n);

}  // namespace s3::game::physics::slide_state::settings
