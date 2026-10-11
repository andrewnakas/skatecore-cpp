// Port of skate-core/src/air/known/output.rs.
#include "s3/core/air/known/output.hpp"

#include <algorithm>
#include <cstdint>

#include "s3/core/air/known/trajectory.hpp"

namespace s3::air::known::output {

using data::Vector4;
using trajectory::add;
using trajectory::sub;
using trajectory::trajectory_position;
using trajectory::trajectory_velocity;

namespace {
constexpr float FIXED_STEP = rs::from_bits(0x3C888889u);  // 0x820849C8, 1/60.
constexpr float MAX_FINITE = rs::from_bits(0x7F7FFFFFu);  // 0x8206D108.
}  // namespace

void fill_physics_output(const data::KnownAirState& state, const data::KnownAirFrame& frame,
                         data::KnownAirOutput& output, runtime::KnownAirRuntime& runtime) {
    const auto prediction = runtime.selected_prediction();
    if ((frame.flags_2468 & 0x00000008u) != 0) {
        output.locked_trajectory_velocity_valid_452 = true;
        const float time = static_cast<float>(state.trajectory_index_216) * FIXED_STEP;
        output.locked_trajectory_velocity_160 = trajectory_velocity(prediction.trajectory, time);
    }

    output.reached_apex_436 = state.reached_apex_208;
    output.jump_height_200 = state.max_y_188 - state.start_y_184;
    output.trajectory_apex_0 = state.trajectory_apex_96;
    output.time_to_apex_196 = state.time_to_apex_200;
    output.collision_position_16 = state.collision_position_112;
    output.landing_normal_32 = state.landing_normal_64;
    output.landing_normal_copy_144 = state.landing_normal_64;
    output.time_until_collision_184 =
        (frame.flags_2468 & 0x00000008u) != 0 ? MAX_FINITE : state.collision_time_196 - state.time_in_state_180;
    output.time_in_state_176 = state.time_in_state_180;
    output.selector_vector_48 = state.selector_vector_128;

    const std::int32_t output_index = std::max<std::int32_t>(state.trajectory_index_216, 1);
    output.trajectory_position_64 =
        trajectory_position(prediction.trajectory, static_cast<float>(output_index) * FIXED_STEP);
    output.landing_heading_80 = state.landing_heading_80;
    output.collision_normal_speed_188 = state.collision_normal_speed_176;
    output.known_air_valid_437 = true;
    output.selected_trajectory_240 = prediction.trajectory;
    output.trajectory_index_220 = state.trajectory_index_216;

    const Vector4 selector_com = runtime.selector_com_position_2784();
    output.selector_com_position_96 = selector_com;
    output.collision_time_180 = state.collision_time_196;
    const auto com_transform = runtime.reckoning_com_transform_816();
    const Vector4 local_com = runtime.transform_point_82d36880(com_transform, selector_com);
    const std::int32_t first_sample = std::max<std::int32_t>(
        static_cast<std::int32_t>(static_cast<std::uint32_t>(state.trajectory_index_216) - 1u), 0);
    for (std::size_t sample_offset = 0; sample_offset < output.trajectory_plane_samples_336.size(); ++sample_offset) {
        const std::int32_t sample_index = static_cast<std::int32_t>(static_cast<std::uint32_t>(first_sample) +
                                                                    static_cast<std::uint32_t>(sample_offset));
        const Vector4 sample = trajectory_position(prediction.trajectory, static_cast<float>(sample_index) * FIXED_STEP);
        const Vector4 relative = sub(add(sample, local_com), state.collision_position_112);
        output.trajectory_plane_samples_336[sample_offset] = runtime.dot3(relative, state.landing_normal_64);
    }
}

}  // namespace s3::air::known::output
