// Port of skate-game/src/physics/player_state/wipeout_output.rs.
//! Wipeout300 Fill82D3EEE8 writes the shared completed physical packet.
#include "s3/game/physics/player_state/wipeout_output.hpp"

#include <utility>

#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/wipeout_states.hpp"

namespace s3::game::physics::player_state::wipeout_output {

void publish(SkaterRuntime& skater) {
    const auto output = wipeout_states::fill(skater);
    auto& physical = skater.player_input.physical;
    auto& skeleton = physical.skeleton;
    skeleton.over_599 = std::uint8_t(output.over_599);
    skeleton.scalar_544 = output.scalar_544;
    skeleton.no_support_time_548 = output.no_support_time_548;
    skeleton.response_strength_580 = output.response_strength_580;
    skeleton.extra_weight_584 = output.extra_weight_584;
    skeleton.time_until_teleport_576 = output.time_until_teleport_576;
    skeleton.teleport_pending_604 = std::uint8_t(output.teleport_pending_604);
    skeleton.hips_right_angle_496 = output.hips_right_angle_496;
    skeleton.hips_up_angle_500 = output.hips_up_angle_500;
    if (output.response_change_588) {
        skeleton.response_change_588 = *output.response_change_588;
        skeleton.response_changed_605 = 1;
    }
    physical.animation.collision_time_144 = output.collision_time_144;
    physical.animation.profile_148 = output.profile_148;
    physical.collision.flag_3479 = std::uint8_t(output.material_ten_3479);
    physical.collision.flag_3480 = std::uint8_t(output.material_eleven_3480);
    physical.collision.flag_3483 = std::uint8_t(output.imminent_surface_twelve_3483);
    physical.collision.predicted_position_64 = rs::to_bits(output.predicted_position_64);
    if (output.retained_air_velocity_160) {
        physical.air.vector_160 = rs::to_bits(*output.retained_air_velocity_160);
        physical.air.use_air_reckoning_452 = 1;
    }
    physical.state.surface_height_32 = output.surface_height_32;
    auto& flags = skater.player_state.state_flags;
    const std::pair<std::size_t, bool> writes[] = {
        {72, output.can_leave_72},
        {81, output.special_surface_81},
        {82, output.below_surface_82},
        {83, output.surface_height_valid_83},
    };
    for (const auto& [offset, value] : writes) flags.at(offset - 52) = value;
    // These source branches only store true, preserving earlier publication.
    if (output.teleport_countdown_68) flags[68 - 52] = true;
    if (output.request_teleport_69) {
        flags[69 - 52] = true;
        physical.state.flag_69 = 1;
    }
}

}  // namespace s3::game::physics::player_state::wipeout_output
