// Port of skate-game/src/physics/known_air/output.rs.
//! Fill82D36880 overwrites every field below except the explicitly retained pair.
#include "s3/game/physics/known_air/output.hpp"

namespace s3::game::physics::known_air::output {

using ::s3::air::known::KnownAirTrajectory;

KnownAirOutput storage(const AirOutputFields& previous) {
    KnownAirOutput o;
    o.trajectory_apex_0 = {0.0f, 0.0f, 0.0f, 0.0f};
    o.collision_position_16 = {0.0f, 0.0f, 0.0f, 0.0f};
    o.landing_normal_32 = {0.0f, 0.0f, 0.0f, 0.0f};
    o.selector_vector_48 = {0.0f, 0.0f, 0.0f, 0.0f};
    o.trajectory_position_64 = {0.0f, 0.0f, 0.0f, 0.0f};
    o.landing_heading_80 = {0.0f, 0.0f, 0.0f, 0.0f};
    o.selector_com_position_96 = {0.0f, 0.0f, 0.0f, 0.0f};
    o.landing_normal_copy_144 = {0.0f, 0.0f, 0.0f, 0.0f};
    o.locked_trajectory_velocity_160 = rs::from_bits(previous.vector_160);
    o.time_in_state_176 = 0.0f;
    o.collision_time_180 = 0.0f;
    o.time_until_collision_184 = 0.0f;
    o.collision_normal_speed_188 = 0.0f;
    o.time_to_apex_196 = 0.0f;
    o.jump_height_200 = 0.0f;
    o.trajectory_index_220 = 0;
    KnownAirTrajectory t;
    t.position = {0.0f, 0.0f, 0.0f, 0.0f};
    t.velocity = {0.0f, 0.0f, 0.0f, 0.0f};
    t.acceleration = {0.0f, 0.0f, 0.0f, 0.0f};
    t.scalar_48 = 0.0f;
    t.word_52 = 0;
    t.word_56 = 0;
    t.word_60 = 0;
    o.selected_trajectory_240 = t;
    o.trajectory_plane_samples_336.fill(0.0f);
    o.reached_apex_436 = false;
    o.known_air_valid_437 = false;
    o.locked_trajectory_velocity_valid_452 = previous.use_air_reckoning_452 != 0;
    return o;
}

void publish(const KnownAirOutput& out, AirOutputFields& physical) {
    physical.trajectory_apex_0 = rs::to_bits(out.trajectory_apex_0);
    physical.collision_position_16 = rs::to_bits(out.collision_position_16);
    physical.landing_normal_32 = rs::to_bits(out.landing_normal_32);
    physical.selector_vector_48 = rs::to_bits(out.selector_vector_48);
    physical.trajectory_position_64 = rs::to_bits(out.trajectory_position_64);
    physical.landing_heading_80 = rs::to_bits(out.landing_heading_80);
    physical.selector_com_position_96 = rs::to_bits(out.selector_com_position_96);
    physical.time_in_state_176 = out.time_in_state_176;
    physical.collision_time_180 = out.collision_time_180;
    physical.collision_normal_speed_188 = out.collision_normal_speed_188;
    physical.time_to_apex_196 = out.time_to_apex_196;
    physical.trajectory_index_220 = out.trajectory_index_220;
    physical.trajectory_plane_samples_336 = out.trajectory_plane_samples_336;
    physical.known_air_valid_437 = std::uint8_t(out.known_air_valid_437);
    const auto t = out.selected_trajectory_240;
    physical.selected_trajectory_240 = {
        rs::to_bits(t.position),
        rs::to_bits(t.velocity),
        rs::to_bits(t.acceleration),
        {rs::to_bits(t.scalar_48), t.word_52, t.word_56, t.word_60},
    };

    physical.vector_160 = rs::to_bits(out.locked_trajectory_velocity_160);
    physical.use_air_reckoning_452 = std::uint8_t(out.locked_trajectory_velocity_valid_452);
    physical.scalar_184 = out.time_until_collision_184;
    physical.landing_normal_144 = rs::to_bits(out.landing_normal_copy_144);
    physical.jump_height_200 = out.jump_height_200;
    physical.reached_apex_436 = std::uint8_t(out.reached_apex_436);
}

}  // namespace s3::game::physics::known_air::output
