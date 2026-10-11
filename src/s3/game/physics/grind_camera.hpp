// Port of skate-game/src/physics/grind_camera.rs.
//! ZIP contact conditioner82DF0640, corroborated against S2 82E2D000.
//! Tangent selection82D87460 belongs to the manager, not to this conditioner.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/player/input_phase/grind_output.hpp"
#include "s3/game/camera.hpp"  // crate::camera::CameraGrindOutput (stage K)

namespace s3::game::physics::grind_camera {

using V = std::array<float, 4>;
using ::s3::game::camera::CameraGrindOutput;

struct Contact {
    V point{};
    V direction{};
    /// Exact processed primitive endpoints, not the board's two truck hits.
    std::array<V, 2> endpoints{};
    std::uint32_t family = 0;
};

struct GrindCamera {
    // Private in Rust.
    V current{};
    V previous{};
    V error{};
    V midpoint{};
    std::uint32_t family = 0;
    bool active = false;

    /// Schedule once after physical FillOut, before camera/graph readers. Uses
    /// retained physical direction, including tipslide's latched drop-in frame,
    /// rather than rereading a reset or newly selected investigation family.
    void condition_fields(::s3::player::input_phase::grind_output::GrindOutputFields& out);
    /// None invalidates history but leaves the caller's retained output alone.
    /// It must not replace inactive direction/target fields with zero.
    void update(const std::optional<Contact>& contact, CameraGrindOutput& output);
};

}  // namespace s3::game::physics::grind_camera
