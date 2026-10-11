// Port of skate-game/src/modding.rs
//! SIMREF: stub for crate::modding::vehicles::network::CollisionShape (skate-game/src/modding/vehicles/network.rs).
//! Vehicles (rapier) are not part of the reference; only the plain data type is kept so
//! physics/network.rs compiles. No vehicle is ever present.
#pragma once
#include <array>

namespace s3::game::modding::vehicles::network {

struct CollisionShape {
    std::array<float, 3> position{};
    std::array<float, 4> rotation{};
    std::array<float, 3> velocity{};
    std::array<float, 3> angular{};
    std::array<float, 3> half{};
    std::array<float, 3> offset{};
    float rounding = 0.0f;
    float mass = 0.0f;
};

}  // namespace s3::game::modding::vehicles::network
