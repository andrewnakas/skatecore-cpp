// Port of skate-core/src/air/known/trajectory.rs.
#include "s3/core/air/known/trajectory.hpp"

#include <cstdint>

namespace s3::air::known::trajectory {

namespace {
constexpr float FIXED_STEP = rs::from_bits(0x3C888889u);                  // 0x820849C8, 1/60.
constexpr float HALF = rs::from_bits(0x3F000000u);                        // 0x8209975C.
constexpr float ONE = rs::from_bits(0x3F800000u);                         // 0x8231A844.
constexpr float PERFECT_FLIP_NUMERATOR = rs::from_bits(0xC0F1463Bu);      // 0x822F9450.
constexpr float NEGATIVE_TWO_PI = rs::from_bits(0xC0C90FDBu);             // 0x822F9434.
constexpr float HEADING_LENGTH_SQUARED_MIN = rs::from_bits(0x3DCCCCCDu);  // 0x820641A8.

std::int32_t wrapping_sub(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}
std::int32_t wrapping_add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
}  // namespace

void init_trajectory_info(KnownAirState& state, const KnownAirFrame& frame, const KnownAirSettings& settings,
                          const KnownAirModeSettings& mode, KnownAirRuntime& runtime) {
    state.landing_normal_64 = frame.selector_landing_normal_2656;

    const auto prediction = runtime.selected_prediction();
    const auto [apex, time_to_apex] = runtime.highest_trajectory_position(prediction.trajectory);
    state.time_to_apex_200 = time_to_apex;
    state.trajectory_apex_96 = apex;

    state.collision_position_112 = runtime.selector_contact_position();
    // 82D35580 reads r30+48; r30 is the complete winning result.
    state.collision_time_196 = prediction.collision_time_48;
    state.body_flip_target_speed_204 = mode.perfect_body_flips_28
                                           ? PERFECT_FLIP_NUMERATOR / state.collision_time_196
                                           : (settings.flip_scalar / state.collision_time_196) * NEGATIVE_TWO_PI;
    state.selector_vector_128 = runtime.selector_vector_2848();

    Vector4 heading{};
    bool heading_valid = false;
    state.collision_normal_speed_176 = 0.0f;
    if (state.collision_time_196 >= 0.0f) {
        const std::int32_t collision_frame = prediction.collision_frame_128;
        const float previous_time = static_cast<float>(wrapping_sub(collision_frame, 2)) * FIXED_STEP;
        const float collision_time = static_cast<float>(wrapping_sub(collision_frame, 1)) * FIXED_STEP;
        const Vector4 previous = trajectory_position(prediction.trajectory, previous_time);
        const Vector4 collision = trajectory_position(prediction.trajectory, collision_time);
        const Vector4 delta = sub(collision, previous);
        const Vector4 velocity = runtime.divide_vector_by_scalar(delta, FIXED_STEP);

        state.collision_normal_speed_176 = runtime.dot3(state.landing_normal_64, velocity);
        const Vector4 normal_velocity = mul_scalar(state.landing_normal_64, state.collision_normal_speed_176);
        const Vector4 tangent = sub(velocity, normal_velocity);
        const auto [normalized, length] = runtime.normalize_safe_with_length(tangent);
        if (!(settings.min_target_heading_velocity_420 > length)) {
            heading = normalized;
            heading_valid = true;
        }
    }

    state.landing_heading_80 = heading;
    state.landing_heading_valid_209 = heading_valid;
    const float heading_length_squared = runtime.dot3(heading, heading);
    if (HEADING_LENGTH_SQUARED_MIN > heading_length_squared) {
        state.landing_heading_valid_209 = false;
    }
}

void restore_velocity(KnownAirState& state, KnownAirFrame& frame, const KnownAirSettings& settings,
                      KnownAirRuntime& runtime) {
    const auto prediction = runtime.selected_prediction();
    if (state.trajectory_index_216 == 0) {
        state.trajectory_index_216 = 1;
    }

    const float sample_time = static_cast<float>(state.trajectory_index_216) * FIXED_STEP;
    const Vector4 velocity = trajectory_velocity(prediction.trajectory, sample_time);
    const auto geometry = runtime.restore_velocity_geometry_82d35998(velocity, frame.ground_normal_464);
    const float curve_value =
        settings.landing_speed_scalar_vs_ground_normal_y_240.evaluate(geometry.landing_speed_curve_input);
    const float lower_clamped =
        select(-geometry.landing_speed_blend_source, 0.0f, geometry.landing_speed_blend_source);
    const float blend_weight = select(ONE - lower_clamped, lower_clamped, ONE);
    const float speed_scale = rs::mul_add(curve_value - ONE, blend_weight, ONE);
    const Vector4 scaled_tangent = mul_scalar(geometry.tangential_velocity, speed_scale);
    const Vector4 current_board_velocity = runtime.board_body_velocity();
    const float preserved_normal_speed = runtime.dot3(current_board_velocity, frame.ground_normal_464);
    Vector4 restored{};
    for (std::size_t lane = 0; lane < 4; ++lane) {
        restored[lane] = rs::mul_add(frame.ground_normal_464[lane], preserved_normal_speed, scaled_tangent[lane]);
    }
    runtime.set_board_velocity(restored);

    const Vector4 forward = runtime.board_forward_axis();
    frame.forward_speed_2612 = runtime.dot3(restored, forward);
}

void update_trajectory_follow(KnownAirState& state, const KnownAirFrame& frame, const KnownAirSettings& settings,
                              const KnownAirRuntime& runtime) {
    state.trajectory_index_216 = wrapping_add(state.trajectory_index_216, 1);

    const float ratio = (settings.trajectory_error_blend_away_time_384 - state.time_in_state_180) /
                        settings.trajectory_error_blend_away_time_384;
    const float lower_clamped = select(-ratio, 0.0f, ratio);
    const float blend = select(ONE - lower_clamped, lower_clamped, ONE);
    const float sample_time = static_cast<float>(state.trajectory_index_216) * FIXED_STEP;
    const KnownAirTrajectory trajectory = runtime.selector_trajectory_2704();
    const Vector4 position = trajectory_position(trajectory, sample_time);
    for (std::size_t lane = 0; lane < 4; ++lane) {
        state.target_com_position_160[lane] =
            rs::mul_add(state.trajectory_follow_offset_144[lane], blend, position[lane]);
    }

    if ((frame.flags_2468 & 0x00000008u) != 0) {
        state.trajectory_index_216 = wrapping_sub(state.trajectory_index_216, 1);
    }
}

Vector4 trajectory_position(const KnownAirTrajectory& trajectory, float time) {
    const float time_squared = time * time;
    Vector4 out{};
    for (std::size_t lane = 0; lane < 4; ++lane) {
        const float linear = rs::mul_add(trajectory.velocity[lane], time, trajectory.position[lane]);
        const float half_acceleration = trajectory.acceleration[lane] * HALF;
        out[lane] = rs::mul_add(half_acceleration, time_squared, linear);
    }
    return out;
}

Vector4 trajectory_velocity(const KnownAirTrajectory& trajectory, float time) {
    Vector4 out{};
    for (std::size_t lane = 0; lane < 4; ++lane) {
        out[lane] = rs::mul_add(trajectory.acceleration[lane], time, trajectory.velocity[lane]);
    }
    return out;
}

}  // namespace s3::air::known::trajectory
