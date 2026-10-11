// Port of skate-core/src/player/input_phase/air_output.rs (whole file).
// Completed Air publication shared by physical state and graph consumers.
#pragma once
#include <array>
#include <cstdint>
#include <tuple>

#include "s3/core/player/input_phase/raw.hpp"
#include "s3/core/rs.hpp"

namespace s3::player::input_phase::air_output {

using types::RawVector;

struct AirOutputFields {
    /// KnownAir Fill82D36880, retained template values outside states that write them.
    RawVector trajectory_apex_0{};
    RawVector collision_position_16{};
    RawVector landing_normal_32{};
    RawVector selector_vector_48{};
    RawVector trajectory_position_64{};
    RawVector landing_heading_80{};
    RawVector selector_com_position_96{};
    RawVector jump_velocity_delta_112{};
    RawVector launch_velocity_128{};
    float time_in_state_176 = 0.0f;
    float collision_time_180 = 0.0f;
    float collision_normal_speed_188 = 0.0f;
    float time_to_apex_196 = 0.0f;
    std::int32_t trajectory_index_220 = 0;
    std::array<RawVector, 4> selected_trajectory_240{};
    std::array<float, 25> trajectory_plane_samples_336{};
    std::uint8_t known_air_valid_437 = 0;
    std::uint8_t launched_442 = 0;
    /// Common ProcessOutput82DB703C: TrajectorySelector9653.
    std::uint8_t flag_443 = 0;
    std::uint8_t flag_444 = 0;
    /// Air324 high bits used by IsHandPlanting82BA55A8.
    std::uint32_t handplant_flags_324 = 0;
    RawVector handplant_position_304{};
    float handplant_time_320 = 0.0f;
    RawVector vector_160{};
    float scalar_184 = 0.0f;
    RawVector landing_normal_144{};
    float jump_height_200 = 0.0f;
    /// FootPlantManager::Fill82D71040: predicted contact and ground-plant duration.
    float footplant_contact_time_208 = 0.0f;
    float footplant_duration_212 = 0.0f;
    float footplant_surface_height_216 = 0.0f;
    std::uint32_t footplant_surface_224 = 0;
    std::uint8_t footplant_left_449 = 0;
    std::uint8_t footplant_right_450 = 0;
    std::uint8_t reached_apex_436 = 0;
    std::uint8_t flag_441 = 0;
    /// 445, the body flip's side, published beside 441 by 82DB6EC0 from the air
    /// reckoning's 1596/1597 pair. The score collector's flip metric 82DA8EB8 turns it
    /// into the sign of its +2348 -- `+1` when this is zero, `-1` otherwise -- which is
    /// what separates a front flip from a back flip.
    std::uint8_t flag_445 = 0;
    std::uint8_t flag_446 = 0;
    std::uint8_t flag_447 = 0;
    std::uint8_t flag_448 = 0;
    std::uint8_t use_air_reckoning_452 = 0;
};
inline bool operator==(const AirOutputFields& a, const AirOutputFields& b) {
    auto t = [](const AirOutputFields& v) {
        return std::tie(v.trajectory_apex_0, v.collision_position_16, v.landing_normal_32, v.selector_vector_48,
                        v.trajectory_position_64, v.landing_heading_80, v.selector_com_position_96,
                        v.jump_velocity_delta_112, v.launch_velocity_128, v.time_in_state_176, v.collision_time_180,
                        v.collision_normal_speed_188, v.time_to_apex_196, v.trajectory_index_220,
                        v.selected_trajectory_240, v.trajectory_plane_samples_336, v.known_air_valid_437,
                        v.launched_442, v.flag_443, v.flag_444, v.handplant_flags_324, v.handplant_position_304,
                        v.handplant_time_320, v.vector_160, v.scalar_184, v.landing_normal_144, v.jump_height_200,
                        v.footplant_contact_time_208, v.footplant_duration_212, v.footplant_surface_height_216,
                        v.footplant_surface_224, v.footplant_left_449, v.footplant_right_450, v.reached_apex_436,
                        v.flag_441, v.flag_445, v.flag_446, v.flag_447, v.flag_448, v.use_air_reckoning_452);
    };
    return t(a) == t(b);
}
inline bool operator!=(const AirOutputFields& a, const AirOutputFields& b) { return !(a == b); }

}  // namespace s3::player::input_phase::air_output
