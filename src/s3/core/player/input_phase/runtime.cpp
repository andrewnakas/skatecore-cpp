// Port of skate-core/src/player/input_phase/runtime.rs (non-generic helpers).
#include "s3/core/player/input_phase/runtime.hpp"

#include "s3/core/player/input_phase/motion_math.hpp"

namespace s3::player::input_phase::runtime::detail {

using publication::replace_bool;
using publication::replace_byte;

void update_signed_ground_time(PlayerInputState& player, const PhysicalPlayerInput& physical,
                               ProcessedPhysicsInput& output) {
    const float previous = player.signed_ground_time_1356;
    player.signed_ground_time_1356 = physical.state.signed_ground_step_84 != 0
                                         ? xenon_fsel(previous, previous, 0.0f) + FIXED_FRAME_STEP
                                         : xenon_fsel(previous, 0.0f, previous) - FIXED_FRAME_STEP;
    output.signed_ground_time_2756 = player.signed_ground_time_1356;
}

void update_time_on_ground(PlayerInputState& player, std::uint32_t captured_category,
                           ProcessedPhysicsInput& output) {
    if (captured_category == 100) {
        output.flags_2472 |= 1u << 13;
        player.grounded_frames_1300 = player.grounded_frames_1300 + 1u;
        player.time_on_ground_1352 += output.timestep_2604;
        replace_bool(player.flags_1296, 31, static_cast<std::int32_t>(player.grounded_frames_1300) > 6);
    } else {
        player.time_on_ground_1352 = 0.0f;
        player.grounded_frames_1300 = 0;
    }
    output.time_on_ground_2752 = player.time_on_ground_1352;
}

void update_timers(PlayerInputState& player, const PhysicalPlayerInput& physical, ProcessedPhysicsInput& output) {
    if (physical.state.state_16 == 104) {
        player.skitch_timer_1376 = 2.0f;
        player.skitch_value_1372 = physical.state.skitch_value_40;
    }
    if (player.skitch_timer_1376 > 0.0f) {
        player.skitch_timer_1376 -= output.timestep_2604;
        output.skitch_value_2592 = player.skitch_value_1372;
    }
    if (physical.ground.scalar_292 > 0.0f) {
        player.ground_timer_1380 = physical.ground.scalar_292;
    }
    if (player.ground_timer_1380 > 0.0f) {
        player.ground_timer_1380 -= output.timestep_2604;
        output.ground_timer_2848 = player.ground_timer_1380;
        output.skitch_value_2592 = player.skitch_value_1372;
    }
    if (physical.ground.scalar_296 > 0.0f) {
        player.secondary_ground_timer_1384 = physical.ground.scalar_296;
    }
    if (player.secondary_ground_timer_1384 > 0.0f) {
        player.secondary_ground_timer_1384 -= output.timestep_2604;
        output.secondary_ground_timer_2852 = player.secondary_ground_timer_1384;
    }
}

void update_ground_history(PlayerInputState& player, std::uint32_t captured_category,
                           const ProcessedPhysicsInput& output) {
    if (captured_category != 200 && captured_category != 600 && player.ground_history_frames_1304 > 0) {
        const auto previous = player.previous_ground_position_1200;
        player.previous_ground_position_1200 = player.current_ground_position_1216;
        if (output.frames_since_teleport_2584 > 1) {
            requests::GroundHistoryRequest request;
            request.previous_position = previous;
            request.current_position = player.current_ground_position_1216;
            request.previous_filtered_delta = player.damped_ground_delta_1248;
            request.timestep_bits = rs::to_bits(output.timestep_2604);
            const auto result = motion_math::ground_history(request);
            player.ground_delta_1232 = result.delta;
            player.damped_ground_delta_1248 = result.filtered_delta;
        }
    }
}

void publish_ground_history(const PlayerInputState& player, ProcessedPhysicsInput& output) {
    output.vectors_464_480_496_512_528[1] = player.previous_ground_position_1200;
    output.vectors_464_480_496_512_528[2] = player.current_ground_position_1216;
    output.vectors_464_480_496_512_528[3] = player.damped_ground_delta_1248;
}

void publish_post_skeleton(PlayerInputState& player, const PhysicalPlayerInput& physical,
                           ProcessedPhysicsInput& output) {
    if ((output.flags_2472 & (1u << 21)) != 0) {
        replace_byte(output.flags_2472, 21, physical.state.flag_66);
    }
    const bool skitch_or_ground_window =
        player.skitch_timer_1376 > 0.0f ||
        ((output.flags_2476 & (1u << 22)) != 0 && physical.ground.scalar_276 >= 0.0f);
    replace_bool(output.flags_2476, 3, skitch_or_ground_window);
    player.time_since_last_input_1348 = (output.flags_2472 & (1u << 23)) != 0
                                            ? player.time_since_last_input_1348 + output.timestep_2604
                                            : 0.0f;
    output.time_since_last_input_2748 = player.time_since_last_input_1348;

    if ((output.flags_2468 & (1u << 21)) != 0) {
        player.prepared_jump_velocity_1184 = output.vectors_400_416[1];
    } else {
        requests::PrepareJumpRequest request;
        request.skateboard_vector = output.vectors_400_416[1];
        request.previous_velocity = player.prepared_jump_velocity_1184;
        player.prepared_jump_velocity_1184 = motion_math::prepared_jump_velocity(request);
    }

    player.spin_same_direction_frames_1324 = output.spin_input_2672 * player.previous_spin_input_1360 <= 0.0f
                                                 ? 0u
                                                 : player.spin_same_direction_frames_1324 + 1u;
    player.previous_spin_input_1360 = output.spin_input_2672;
    output.spin_same_direction_frames_2580 = player.spin_same_direction_frames_1324;
    output.prepared_jump_704 = player.prepared_jump_velocity_1184;
    output.crouch_delta_2780 = output.crouch_2776 - player.previous_crouch_1364;
    player.previous_crouch_1364 = output.crouch_2776;
}

std::uint32_t native_nonnegative_decrement(std::uint32_t value) {
    const std::uint32_t decremented = value - 1u;
    // `decremented.overflowing_add(0x8000_0000).1`: overflow iff the high bit is set.
    if (decremented >= 0x80000000u) {
        return 0;
    }
    return decremented;
}

}  // namespace s3::player::input_phase::runtime::detail
