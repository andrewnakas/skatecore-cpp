// Port of skate-core/src/player/wipeout_state/output.rs (whole file).
// Physical output stores82D3EEE8. Optional fields are conditional writes,
// not instructions to clear the corresponding shared output on false paths.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/player/wipeout_state/data.hpp"

namespace s3::player::wipeout_state::output {

struct Output {
    bool over_599 = false;
    float scalar_544 = 0.0f;
    float no_support_time_548 = 0.0f;
    bool can_leave_72 = false;
    bool special_surface_81 = false;
    bool below_surface_82 = false;
    float surface_height_32 = 0.0f;
    bool surface_height_valid_83 = false;
    float collision_time_144 = 0.0f;
    std::uint32_t profile_148 = 0;
    float response_strength_580 = 0.0f;
    float extra_weight_584 = 0.0f;
    float time_until_teleport_576 = 0.0f;
    bool teleport_pending_604 = false;
    bool material_ten_3479 = false;
    bool material_eleven_3480 = false;
    bool imminent_surface_twelve_3483 = false;
    std::array<float, 4> predicted_position_64{};
    std::optional<std::array<float, 4>> retained_air_velocity_160;
    std::optional<float> response_change_588;
    bool teleport_countdown_68 = false;
    bool request_teleport_69 = false;
    float hips_right_angle_496 = 0.0f;
    float hips_up_angle_500 = 0.0f;
};

}  // namespace s3::player::wipeout_state::output
