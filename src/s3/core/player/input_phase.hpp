// PARTIAL PORT (stage C): only items used by physics.
// Port of skate-core/src/player/input_phase/{mod,types}.rs.
//
// Skate 3 TU3 `PhysicalPlayerHiLOD::Input` (`0x82DB4048`) packet types. Stage C
// (physics::board_input_output) only needs the board-owned fields of
// `PhysicalPlayerInput`; the field structs below are complete ports of their Rust
// definitions, while `PhysicalPlayerInput` omits the members owned by other
// stages (air, physics, grinds, skeleton, animation, scoring, teleport_output,
// state, reckoning, off_board). Stage P should extend this header in place.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/rs.hpp"

namespace s3::player::input_phase {

using RawVector = std::array<std::uint32_t, 4>;
using RawMatrix = std::array<std::array<std::uint32_t, 4>, 4>;

struct SkateboardMotionFields {
    RawVector vector_64{};
    RawVector vector_80{};
    RawVector vector_96{};
    RawVector vector_128{};
    RawVector vector_144{};
    float scalar_160 = 0.0f;
    float scalar_164 = 0.0f;
    float scalar_168 = 0.0f;
    float scalar_172 = 0.0f;
};
S3_DERIVE_EQ(SkateboardMotionFields, vector_64, vector_80, vector_96, vector_128, vector_144, scalar_160, scalar_164,
             scalar_168, scalar_172)

struct GroundOutputFields {
    /// State503 Fill82D4E090..E0C8.
    std::int32_t landing_half_turns_312 = 0;
    std::uint8_t hippy_jumping_322 = 0;
    std::uint8_t hippy_takeoff_323 = 0;
    RawVector vector_64{};
    /// Board Fill82C03318: wheel-contact normal (not Motion80 velocity).
    RawVector vector_80{};
    /// Host storage for Motion273, NOT native Ground273. Common82DB7598
    /// publishes Processed2468 bit20; keep this distinct from the normal.
    std::uint8_t flag_273 = 0;
    RawVector vector_96{};
    RawVector vector_128{};
    float scalar_276 = 0.0f;
    float scalar_292 = 0.0f;
    float scalar_296 = 0.0f;
    std::uint8_t flag_317 = 0;
    std::uint8_t flag_318 = 0;
};
S3_DERIVE_EQ(GroundOutputFields, landing_half_turns_312, hippy_jumping_322, hippy_takeoff_323, vector_64, vector_80,
             flag_273, vector_96, vector_128, scalar_276, scalar_292, scalar_296, flag_317, flag_318)

struct CollisionOutputFields {
    std::uint32_t wheel_count_0 = 0;
    float scalar_28 = 0.0f;
    /// Offboard landing manager82D79498; reset82DE3290 clears this position.
    RawVector vector_48{};
    RawVector predicted_position_64{};
    std::uint8_t flag_215 = 0;
    std::uint8_t flag_216 = 0;
    std::array<std::uint8_t, 4> wheel_contact_3296_3299{};
    std::uint8_t flag_3472 = 0;
    std::uint8_t flag_3475 = 0;
    std::uint8_t flag_3477 = 0;
    std::uint8_t flag_3478 = 0;
    std::uint8_t flag_3479 = 0;
    std::uint8_t flag_3480 = 0;
    std::uint8_t flag_3481 = 0;
    /// Offboard landing manager82D7948C; reset82DE32E4 clears availability.
    std::uint8_t flag_3482 = 0;
    std::uint8_t flag_3483 = 0;
};
S3_DERIVE_EQ(CollisionOutputFields, wheel_count_0, scalar_28, vector_48, predicted_position_64, flag_215, flag_216,
             wheel_contact_3296_3299, flag_3472, flag_3475, flag_3477, flag_3478, flag_3479, flag_3480, flag_3481,
             flag_3482, flag_3483)

/// PARTIAL: see the header comment for the omitted members.
struct PhysicalPlayerInput {
    /// SkateboardReckoning+176; restored r29 at82DB4994 is176.
    RawVector board_reckoning_side_176{};
    SkateboardMotionFields skateboard;
    // air, physics, grinds, skeleton, animation, scoring, teleport_output: not yet ported.
    CollisionOutputFields collision;
    // state: not yet ported.
    GroundOutputFields ground;
    // reckoning: not yet ported.
    std::uint32_t filtered_state_0 = 0;
    // off_board: not yet ported.
    /// `player+1896 -> +0 -> +24 -> +16`, before the variant override.
    std::uint32_t surface_default_mode = 0;
    /// Player component +1832, word +1876; source bit 28 is consumed.
    std::uint32_t component_1832_word_1876 = 0;
};

}  // namespace s3::player::input_phase
