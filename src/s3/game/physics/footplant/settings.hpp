// Port of skate-game/src/physics/footplant/settings.rs.
//! Cached physics_footplantmanager layout used by82D6FD60/82D6F5F0/82D70070.
#pragma once
#include <array>
#include <string>

#include "s3/core/point_graph.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::footplant::settings {

/// `pub(super)`.
struct Settings {
    std::array<float, 4> deck_bounds{};                     //0
    float max_leg_angle_error = 0.0f;                       //280 degrees
    float leg_length_on_landing = 0.0f;                     //288
    float foot_volume_y_offset = 0.0f;                      //296
    float deck_bounds_y_offset = 0.0f;                      //300
    sc::point_graph::PointGraph<8> radial_speed_scale{};    //16
    float max_horizontal_speed = 0.0f;                      //256
    float max_descending_speed = 0.0f;                      //260
    float release_outward_speed = 0.0f;                     //268
    float end_handle = 0.0f;                                //272
    float end_angle = 0.0f;                                 //276 degrees
    float end_leg_length = 0.0f;                            //284
    float start_handle = 0.0f;                              //292
    float min_duration = 0.0f;                              //304
    float max_duration = 0.0f;                              //308

    static rs::Result<Settings, std::string> load(const sd::Collections& data);
};

}  // namespace s3::game::physics::footplant::settings
