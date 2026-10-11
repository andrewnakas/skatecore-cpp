// Port of skate-game/src/physics/air_trajectory/settings.rs.
//! Stock class hashes429A8F4385DD539D(trajectory),7CE1B6B581709C6F
//! (reckoning),5338958C408B6009(feet), defaultD7EDBD362D7D2152.
//! Layout names/offsets were checked against original TU3 loads and stock XML.
#pragma once
#include <string>

#include "s3/core/air/trajectory.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::air_trajectory::settings {

rs::Result<::s3::air::trajectory::SelectorSettings, std::string> load(const sd::Collections& data);

// Private helpers in Rust.
rs::Result<::s3::point_graph::PointGraph<8>, std::string> graph8(const sd::Collections& data, const std::string& class_,
                                                                 const std::string& name, bool negative);
rs::Result<::s3::point_graph::PointGraph<4>, std::string> negative_graph4(const sd::Collections& data,
                                                                         const std::string& class_,
                                                                         const std::string& name);

}  // namespace s3::game::physics::air_trajectory::settings
