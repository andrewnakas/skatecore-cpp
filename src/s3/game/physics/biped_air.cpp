// Port of skate-game/src/physics/biped_air.rs
//! Distinct State501 lifecycle: original S3 82D2E5C0/EF38/FFF8/30300/EF20.
#include "s3/game/physics/biped_air.hpp"

#include <utility>

#include "s3/game/physics.hpp"
#include "s3/game/physics/offboard/air_feet.hpp"
#include "s3/game/physics/offboard/landing_deck.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::biped_air {

using ::s3::player::offboard::controller::PlacementInput;

rs::Result<BipedAir, std::string> BipedAir::load(const sd::Collections& data) {
    S3G_TRY(auto checks, settings::load(data));
    BipedAir out{};
    out.checks = checks;
    return out;
}

///82DB3C78 at the early fixed-step phase, before current ProcessInput.
///82859E70 completes earlier trajectory work atA06C/A080 and invokes virtual
///slot16 atA0D0. Actual vtable82328688+16 points to82DB3C78.
Status consume_selector(const GamePhysics& physics, SkaterRuntime& skater) {
    const auto context = input::context(skater);
    //82C209E4 supplies the stock Air clearance parameter, not guessed geometry.
    const float clearance = skater.offboard_air_selector.settings.deck_center_to_truck;
    auto r = skater.offboard_air_selector.consume(physics.world, context, clearance);
    if (r.is_err()) return rs::make_err(std::string(r.unwrap_err()));
    return ok();
}

Status enter(const GamePhysics& physics, SkaterRuntime& skater) {
    skater.biped_air.state.reset();
    //State68 is the SAME manager consumed by state503; state64 is Biped.
    skater.landing_deck.reset();
    const auto entry = input::enter(skater);
    skater.biped_air.state.begin_enter_after_reset(entry);
    auto feet = std::exchange(skater.offboard_feet, {});
    offboard::air_feet::enter(skater, feet);
    skater.offboard_feet = feet;
    S3G_CHECK(skater.ground_lifecycle.skeleton_controller.request(4, skater.skeleton_collision));
    if (!skater.offboard_air_selector.core.sampling.preinitialized_8494) {
        S3G_TRY(const auto packet, input::launch_packet(skater));
        S3G_CHECK(input::submit(physics, skater, packet));
    }
    const auto& p = skater.player_input.processed;
    //82D7B7A0 is the common Biped placement, not Ground Enter/Update.
    PlacementInput place{};
    place.frame = entry.animation_frame;
    place.velocity = rs::from_bits(p.vectors_544_560_592_608[3]);
    place.body_position = entry.body_position_15872;
    place.current_state = p.state_2508;
    place.previous_state = p.state_2504;
    for (std::size_t i = 0; i < 4; ++i) place.previous_frame[i] = rs::from_bits(p.effective_anim_transform_192[i]);
    skater.biped_ground.controller.place(place);
    skater.biped_air.state.finish_enter(input::enter(skater));
    return ok();
}

void exit(SkaterRuntime& skater) { skater.biped_air.state.exit(skater.offboard_air_selector.core.sampling); }

Status fill(SkaterRuntime& skater) {
    auto& physical = skater.player_input.physical;
    publication::publish(skater.biped_air.state.output(), physical.off_board);
    offboard::air_feet::publish(skater.offboard_feet, physical.off_board);
    //82D7948C/98: destination is PhysOut24 collision, NOT Skeleton20.
    skater.landing_deck.publish(offboard::landing_deck::PublishTargets{
        &physical.off_board, &physical.collision.vector_48, &physical.collision.flag_3482});
    return ok();
}

}  // namespace s3::game::physics::biped_air
