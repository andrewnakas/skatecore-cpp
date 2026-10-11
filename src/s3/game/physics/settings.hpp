// Port of skate-game/src/physics/settings.rs.
//! Stock skater XML values bound to the recovered physical calculations.
#pragma once
#include <array>
#include <string>

#include "s3/core/math.hpp"
#include "s3/core/physics/board_step.hpp"
#include "s3/core/physics/contact.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/physics/drive_parameters.hpp"
#include "s3/core/physics/mass.hpp"
#include "s3/core/physics/rigid_body.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::settings {

struct PhysicsSettings {
    sc::physics::board_step::BoardStepSettings step{};
    std::array<sc::physics::rigid_body::RetailBodyMassProperties, 7> masses{};
    std::array<sc::physics::drive_frames::RetailAffineTransform, 7> authored{};
    sc::physics::mass::DeckGeometry deck_geometry{};
    sc::physics::mass::MassShape truck_shape{};
    bool truck_collisions = false;
    sc::physics::contact::RetailContactMaterial truck_material{};
    sc::physics::contact::RetailContactMaterial deck_material{};
    float wheel_radius = 0.0f;
    sc::physics::contact::RetailContactMaterial wheel_material{};
    sc::physics::contact::RetailContactMaterial standard_wheel_material{};
    sc::physics::contact::RetailContactMaterial floor_material{};
    float input_magnitude_threshold = 0.0f;

    static rs::Result<PhysicsSettings, std::string> load(const sd::Collections& data);
};

}  // namespace s3::game::physics::settings

namespace s3::game::physics {
using settings::PhysicsSettings;
}
