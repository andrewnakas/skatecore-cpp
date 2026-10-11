// Port of skate-game/src/physics/wipeout_states/settings.rs
//! Stock vault bindings for WipeoutGround300 and its authored control profiles.
#pragma once
#include <array>
#include <string>
#include <string_view>

#include "s3/core/physics/contact.hpp"
#include "s3/core/player/wipeout_state.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::wipeout_states::settings {

struct Settings {
    ::s3::player::wipeout_state::recovery::Settings recovery{};
    float remove_target_time = 0.0f;
    float remove_drives_time = 0.0f;
    float controlled_weight_step = 0.0f;
    float collision_weight_step = 0.0f;
    float board_restitution = 0.0f;
    float board_friction = 0.0f;
    float deck_angular_drag = 0.0f;
    float push_force = 0.0f;
    std::array<::s3::physics::contact::RetailContactMaterial, 3> standard_materials{};

    static rs::Result<Settings, std::string> load(const sd::Collections& data);
};

rs::Result<::s3::physics::contact::RetailContactMaterial, std::string> material(const sd::Collections& data,
                                                                               std::string_view class_,
                                                                               std::string_view prefix);

/// The key is supplied by the original constructor's verified handle binding.
/// PointNegGraph metadata occupies four words before the eight X/Y samples.
rs::Result<::s3::player::wipeout_state::profiles::Profile, std::string> load_profile(const sd::Collections& data,
                                                                                    std::string_view key);

}  // namespace s3::game::physics::wipeout_states::settings
