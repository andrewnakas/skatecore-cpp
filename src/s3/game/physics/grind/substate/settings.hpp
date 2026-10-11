// Port of skate-game/src/physics/grind/substate/settings.rs.
//! Stock mode endpoints and constructor-vtable-resolved jump getters.
#pragma once
#include <array>
#include <cstdint>
#include <string>

#include "s3/core/point_graph.hpp"
#include "s3/core/riding/collision_response.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/grind/family.hpp"

namespace s3::game::physics::grind::substate::settings {

struct Settings {
    ::s3::riding::collision_response::CollisionResponseSettings collision{};
    float animated_board_threshold = 0.0f;
    //Mode order is the existing Processed.state_variant_index_2528 contract.
    std::array<std::array<std::array<float, 2>, 3>, 5> vertical_{};  // private `vertical` (name clashes with the getter)

    static rs::Result<Settings, std::string> load(const sd::Collections& data);
    rs::Result<float, std::string> vertical(Family family, std::uint32_t mode, float strength) const;
};

float side(Family family, std::uint32_t geometry_kind);

}  // namespace s3::game::physics::grind::substate::settings
