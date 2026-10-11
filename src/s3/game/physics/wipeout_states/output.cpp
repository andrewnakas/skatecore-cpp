// Port of skate-game/src/physics/wipeout_states/output.rs
#include "s3/game/physics/wipeout_states/output.hpp"

#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::wipeout_states::output {

::s3::player::wipeout_state::output::Output fill(const SkaterRuntime& skater) {
    const auto& runtime = skater.wipeout_state;
    const auto& feedback = skater.collision_feedback;
    const auto normal = ::s3::player::wipeout_state::response::response_normal(
        feedback.flags.compliant ? std::optional<std::array<float, 4>>(feedback.highest_normal) : std::nullopt,
        runtime.prediction.result.valid()
            ? std::optional<std::array<float, 4>>(runtime.prediction.result.landing_normal)
            : std::nullopt);
    return runtime.state.output(skater.skeleton.record.pose[23], normal);
}

}  // namespace s3::game::physics::wipeout_states::output
