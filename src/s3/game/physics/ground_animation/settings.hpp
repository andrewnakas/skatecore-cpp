// Port of skate-game/src/physics/ground_animation/settings.rs.
//!82D94C10 copies physics_jump layout0..456 into the board toolkit cache.
#pragma once
#include <array>
#include <string>
#include <string_view>

#include "s3/core/air/ground_jump.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::ground_animation::settings {

struct GroundAnimationSettings {
    sc::air::ground_jump::GroundJumpSettings jump{};
    std::array<sc::air::ground_jump::GroundJumpMode, 5> modes{};

    static rs::Result<GroundAnimationSettings, std::string> load(const sd::Collections& d);
};

/// Private in Rust.
rs::Result<sc::point_graph::PointGraph<8>, std::string> graph(const sd::Collections& d, std::string_view name,
                                                              bool negative);

}  // namespace s3::game::physics::ground_animation::settings
