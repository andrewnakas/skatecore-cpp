// Port of skate-game/src/physics/grind_materials.rs.
//!82D89DC0/82C090C0 material modes, applied to the live board collider slots.
//! Init82C08C28 clears all three grind materials at82C09084..A4; S2 Init
//!82B374E8 confirms their identities. Grind resistance is a separate force.
#pragma once
#include <array>

#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/contact.hpp"
#include "s3/game/physics/player_input/grind.hpp"  // MaterialMode (group C)

namespace s3::game::physics::settings {
struct PhysicsSettings;  // physics/settings.hpp (coordinator)
}

namespace s3::game::physics::grind_materials {

using ::s3::physics::board_runtime::BoardRuntime;
using ::s3::physics::contact::RetailContactMaterial;
using player_input::grind::MaterialMode;

struct GrindMaterials {
    std::array<RetailContactMaterial, 3> standard{};  // private in Rust

    static GrindMaterials new_(const settings::PhysicsSettings& settings);
    void apply(MaterialMode mode, BoardRuntime& board, settings::PhysicsSettings& settings) const;
};

}  // namespace s3::game::physics::grind_materials
