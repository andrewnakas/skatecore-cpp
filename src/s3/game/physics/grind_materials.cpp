// Port of skate-game/src/physics/grind_materials.rs.
//!82D89DC0/82C090C0 material modes, applied to the live board collider slots.
//! Init82C08C28 clears all three grind materials at82C09084..A4; S2 Init
//!82B374E8 confirms their identities. Grind resistance is a separate force.
#include "s3/game/physics/grind_materials.hpp"

#include "s3/game/physics/settings.hpp"

namespace s3::game::physics::grind_materials {

GrindMaterials GrindMaterials::new_(const settings::PhysicsSettings& settings) {
    GrindMaterials out;
    out.standard = {settings.standard_wheel_material, settings.truck_material, settings.deck_material};
    return out;
}

void GrindMaterials::apply(MaterialMode mode, BoardRuntime& board, settings::PhysicsSettings& settings) const {
    std::array<RetailContactMaterial, 3> materials{};
    switch (mode) {
        case MaterialMode::Unchanged:
            return;
        case MaterialMode::Standard:
            materials = standard;
            break;
        case MaterialMode::Grind: {
            const RetailContactMaterial zero{0.0f, 0.0f, 0.0f};
            materials = {zero, zero, zero};
            break;
        }
    }
    board.set_collision_group(4);
    settings.wheel_material = materials[0];
    settings.truck_material = materials[1];
    settings.deck_material = materials[2];
    // These leaves do not change shape enable flags, angular drag, motion
    // activation or wiping-out state. They are not full SetStandard82C03AF8.
}

}  // namespace s3::game::physics::grind_materials
