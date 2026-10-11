// Port of skate-game/src/physics/grind/observation.rs.
//! Required producer inputs. None implements Default: absence of a producer is
//! not permission to use world-up, metal friction, geometry zero or full energy.
//! (C++: members are value-initialised only so the structs are constructible.)
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/game/physics/grind/family.hpp"

namespace s3::game::physics::grind::observation {

using V = std::array<float, 4>;

struct GeometryObservation {
    V point_1120{};
    V direction_1136{};
    V normal_1152{};
    V target_up_1168{};
    V primitive_start_1264{};
    V primitive_end_1280{};
    /// Native optional spline header contains TWO independent GUIDs.
    std::optional<std::array<std::uint64_t, 2>> spline_guids_1296;
    V upmost_normal_1408{};
    V high_side_1440{};
    /// Retain every value; force leaves select 0, 1, or all other geometry.
    std::uint32_t kind_1464 = 0;
    std::uint32_t flags_1476 = 0;
    float impact_speed_1492 = 0.0f;
};

struct SurfaceObservation {
    /// Output/audio identifier is NOT the friction material identifier.
    std::uint32_t audio_surface_1468 = 0;
    std::uint32_t material_1472 = 0;
    float friction_vs_time_1496 = 0.0f;
    float reckon_blend_selector_1500 = 0.0f;
    /// S3 gravity-relief producer82D8ACF0; NOT UpdateFrictionVsTime.
    float gravity_relief_1512 = 0.0f;
};

struct ControlObservation {
    Family family = Family::FiftyFifty;
    /// Signed bit31 forces exit; bit29 selects the front end; bit30 delivers
    /// engagement velocity. Keep the source word rather than guessing booleans.
    std::uint32_t flags_1516 = 0;
    std::uint32_t flags_2468 = 0;
    std::uint32_t flags_2488 = 0;
    float translation_2796 = 0.0f;
    float balance_2800 = 0.0f;
    /// Original exit-lean producer's result, not derived from deck lean here.
    float exit_lean = 0.0f;
};

struct EngagementObservation {
    V velocity_1184{};
    std::uint32_t kind_1248 = 0;
};

/// Jumper cache after82D739D8; invalid investigation retains its previous
/// geometry/normals. Do not reconstruct these from the current deck frame.
struct JumperObservation {
    std::uint32_t geometry_kind_16 = 0;
    Family family_20 = Family::FiftyFifty;
    float energy_24 = 0.0f;
    V high_side_32{};
    V normal_48{};
    V direction_64{};
    V upmost_normal_80{};
    V point_96{};
};

/// Snapshot after the manager's geometry, material, assistance, balance,
/// engagement, controls and jumper phases. Production wiring belongs to main.
struct ManagerObservation {
    GeometryObservation geometry;
    SurfaceObservation surface;
    ControlObservation control;
    EngagementObservation engagement;
    JumperObservation jumper;
};

}  // namespace s3::game::physics::grind::observation
