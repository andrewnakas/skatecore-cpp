// Port of skate-core/src/player/input_phase/grind_input.rs (whole file).
// Value snapshot copied by82D87E80 into Processed1104..1519 after manager post.
// Native pointers are stable host identities; all geometry/control values are
// retained independently. Zero is the native416-byte investigation reset.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/player/input_phase/raw.hpp"
#include "s3/core/rs.hpp"

namespace s3::player::input_phase::grind_input {

using types::RawVector;

struct GrindInvestigationFields {
    RawVector tangent_1104{};
    RawVector point_1120{};
    RawVector direction_1136{};
    RawVector normal_1152{};
    RawVector target_up_1168{};
    RawVector entry_velocity_1184{};
    RawVector front_contact_1200{};
    RawVector rear_contact_1216{};
    RawVector vector_1232{};
    std::uint32_t family_1248 = 0;
    std::uint32_t entry_kind_1252 = 0;
    RawVector primitive_start_1264{};
    RawVector primitive_end_1280{};
    std::optional<std::uint64_t> owner_1296{};
    std::uint32_t primitive_flags_1300 = 0;
    RawVector second_start_1312{};
    RawVector second_end_1328{};
    std::optional<std::uint64_t> second_owner_1344{};
    std::uint32_t second_flags_1348 = 0;
    RawVector center_1360{};
    std::array<RawVector, 2> far_points_1376_1392{};
    RawVector upmost_normal_1408{};
    RawVector surface_direction_1424{};
    RawVector high_side_1440{};
    std::array<float, 2> normal_limits_1456_1460{};
    std::uint32_t geometry_kind_1464 = 0;
    std::uint32_t audio_surface_1468 = 0;
    std::uint32_t physics_surface_1472 = 0;
    std::uint32_t geometry_flags_1476 = 0;
    bool valid_1488 = false;
    float impact_speed_1492 = 0.0f;
    float friction_1496 = 0.0f;
    float exit_lean_1500 = 0.0f;
    float yaw_1504 = 0.0f;
    float pitch_1508 = 0.0f;
    float gravity_relief_1512 = 0.0f;
    std::uint32_t flags_1516 = 0;
};
S3_DERIVE_EQ(GrindInvestigationFields, tangent_1104, point_1120, direction_1136, normal_1152, target_up_1168,
             entry_velocity_1184, front_contact_1200, rear_contact_1216, vector_1232, family_1248, entry_kind_1252,
             primitive_start_1264, primitive_end_1280, owner_1296, primitive_flags_1300, second_start_1312,
             second_end_1328, second_owner_1344, second_flags_1348, center_1360, far_points_1376_1392,
             upmost_normal_1408, surface_direction_1424, high_side_1440, normal_limits_1456_1460, geometry_kind_1464,
             audio_surface_1468, physics_surface_1472, geometry_flags_1476, valid_1488, impact_speed_1492,
             friction_1496, exit_lean_1500, yaw_1504, pitch_1508, gravity_relief_1512, flags_1516)

}  // namespace s3::player::input_phase::grind_input
