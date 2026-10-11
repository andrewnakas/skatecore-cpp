// Port of skate-game/src/graph_host/motion_score_execute.rs (module `motion::score_execution`).
#include "s3/game/graph_host/motion/score_execution.hpp"

#include "s3/core/animation/playback_parameters.hpp"
#include "s3/game/graph_host/motion/execution.hpp"

namespace s3::game::graph_host::motion {

rs::Result<bool, std::string> MotionHost::execute_score_or_bump(const MotionOperation& operation,
                                                                std::uint8_t phase) {
    using s3::animation::playback_parameters::SettableAttribute;
    if (const auto* scoring = std::get_if<MotionOperation::ScoringTrick>(&operation.value)) {
        scoring->_0.execute(score_packet.trick_names, score_packet.flags, phase);
    } else if (const auto* bump = std::get_if<MotionOperation::SetBumpCoefficients>(&operation.value)) {
        if (phase == 0) {
            S3_MHOST_SOME(const auto acceleration, bump_acceleration,
                           "SetBumpCoefficients requires completed acceleration");
            S3_MHOST_SOME(const std::uint32_t animation_flags, animation.skater_animation_flags,
                           "SetBumpCoefficients requires actual stance");
            const std::array<float, 2> values =
                bump_settings.coefficients(acceleration, (animation_flags & 0x40000000u) != 0);
            animation.set_attribute(SettableAttribute{bump->x, values[0], false, -1});
            animation.set_attribute(SettableAttribute{bump->y, values[1], false, -1});
        }
    } else {
        return false;
    }
    return true;
}

}  // namespace s3::game::graph_host::motion
