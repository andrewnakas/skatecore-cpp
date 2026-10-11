// Port of skate-core/src/player/offboard/landing_deck/types.rs (whole file).
// Typed boundaries of original82D78xxx/82D79xxx. No synthetic input defaults.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/player/_deps/air_trajectory.hpp"
#include "s3/core/rs.hpp"

namespace s3::player::offboard::landing_deck {

using air::trajectory::QueryRequest;
using Vector = std::array<float, 4>;
using Frame = std::array<Vector, 4>;

struct Settings {
    ///physics_landingondeck layout+4; strict board-up.Y comparison.
    float deck_min_uprightness = 0.0f;
    ///physics_landingondeck layout+24; plane point=deck112+up544*height.
    float approximate_com_height = 0.0f;
};
S3_DERIVE_EQ(Settings, deck_min_uprightness, approximate_com_height)

struct Input {
    Vector board_up_80{};
    Vector board_position_112{};
    Vector board_velocity_400{};
    Vector up_544{};
    std::int32_t support_1776 = 0;
    std::uint32_t flags_2480 = 0;
    std::uint32_t mode_2540 = 0;
    std::uint32_t wheel_contacts_2556 = 0;
};
S3_DERIVE_EQ(Input, board_up_80, board_position_112, board_velocity_400, up_544, support_1776, flags_2480, mode_2540,
             wheel_contacts_2556)

struct AssistInput {
    Input processed{};
    Vector position{};
    Vector velocity{};
    float maximum_velocity_change = 0.0f;
};
S3_DERIVE_EQ(AssistInput, processed, position, velocity, maximum_velocity_change)

struct IkInput {
    Input processed{};
    float time_to_land = 0.0f;
    Frame animation_root{};
    Vector animation_com_10960{};
    Vector mapped_position_12608{};
};
S3_DERIVE_EQ(IkInput, processed, time_to_land, animation_root, animation_com_10960, mapped_position_12608)

struct SyncInput {
    Vector position_592{};
    std::uint32_t flags_2488 = 0;
};
S3_DERIVE_EQ(SyncInput, position_592, flags_2488)

///Complete numeric result82D794A0; offsets describe original output, not Rust ABI.
struct UpdateOutput {
    bool can_land = false;          // 0
    bool trajectory_valid = false;  // 1
    float time_to_land = 0.0f;      // 4
    float elapsed = 0.0f;           // 8
    float landing_time = 0.0f;      // 12
    float apex_time = 0.0f;         // 16
    Vector position{};              // 32
    Vector landing_velocity{};      // 48
    Vector launch_position{};       // 64
    Vector landing_position{};      // 80
    Vector normal{};                // 96
    Vector apex_position{};         // 112
    Vector direction{};             // 128
    Vector up{};                    // 144
    ///Host must additionally use actual processed filter2952 and submit before
    ///query_submitted(). Native request has radius.3, start/end error.5.
    std::optional<QueryRequest> query;
};
S3_DERIVE_EQ(UpdateOutput, can_land, trajectory_valid, time_to_land, elapsed, landing_time, apex_time, position,
             landing_velocity, launch_position, landing_position, normal, apex_position, direction, up, query)

struct FillOutput {
    bool can_land_316 = false;
    bool hippy_hurdling_317 = false;
    ///Some sets skeleton3482 and writes skeleton48; None leaves both untouched.
    std::optional<Vector> moving_contact;
};
S3_DERIVE_EQ(FillOutput, can_land_316, hippy_hurdling_317, moving_contact)

}  // namespace s3::player::offboard::landing_deck
