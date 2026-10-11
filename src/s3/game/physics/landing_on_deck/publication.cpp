// Port of skate-game/src/physics/landing_on_deck/publication.rs
//! State503 FillPhysOut82D4DFC8. No output is inferred from render transforms.
#include "s3/game/physics/landing_on_deck/publication.hpp"

#include "s3/game/physics/offboard/landing_deck.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::landing_on_deck::publication {

Status fill(SkaterRuntime& skater) {
    const auto& state = skater.landing_on_deck.state;
    S3G_TRY_OPT(const auto* output, state.output ? &*state.output : nullptr,
                "Landing Fill requires its completed Update");
    auto& physical = skater.player_input.physical;
    auto& off = physical.off_board;
    off.trajectory_valid_331 = static_cast<std::uint8_t>(output->trajectory_valid);
    off.scalar_152 = output->elapsed;
    off.vector_176 = rs::to_bits(output->launch_position);
    off.vector_208 = rs::to_bits(output->landing_position);
    off.scalar_92 = output->landing_time;
    off.vector_192 = rs::to_bits(output->normal);
    off.vector_240 = rs::to_bits(output->apex_position);
    off.scalar_156 = output->apex_time;
    off.vector_224 = rs::to_bits(output->direction);
    off.vector_160 = rs::to_bits(output->up);
    off.scalar_32 = state.time_to_land;
    off.flag_315 = static_cast<std::uint8_t>(state.near_deck);
    off.flag_318 = static_cast<std::uint8_t>(state.turning);
    physical.ground.hippy_takeoff_323 = static_cast<std::uint8_t>(state.takeoff_frames > 0);
    physical.ground.hippy_jumping_322 = static_cast<std::uint8_t>(state.hippy);
    physical.ground.landing_half_turns_312 = state.landing_half_turns();
    skater.landing_deck.publish(
        offboard::landing_deck::PublishTargets{&off, &physical.collision.vector_48, &physical.collision.flag_3482});
    return ok();
}

}  // namespace s3::game::physics::landing_on_deck::publication
