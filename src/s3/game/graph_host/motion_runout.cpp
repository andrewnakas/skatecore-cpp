// Port of skate-game/src/graph_host/motion_offboard/runout.rs (module `motion_runout`).
#include "s3/game/graph_host/motion_runout.hpp"

#include "s3/core/player/wipeout_state/math.hpp"
#include "s3/core/player/wipeout_state/orientation.hpp"

namespace s3::game::graph_host::motion_runout {

namespace math = s3::player::wipeout_state::math;
namespace orientation = s3::player::wipeout_state::orientation;

void State::begin(std::optional<Observation> observation) {
    if (!observation) return;
    const std::array<float, 4> velocity =
        observation->offboard_flag_331 ? observation->offboard_velocity_128 : observation->reckoning_velocity_16;
    float angle = orientation::projected_angle(observation->skeleton_vector_0, velocity, observation->reckoning_up_96);
    if (observation->animation_mirrored) angle = -angle;
    speed_12 = math::length(velocity);
    angle_degrees_8 = math::wrap_angle(angle) * rs::from_bits(0x42652EE1u);
}

}  // namespace s3::game::graph_host::motion_runout
