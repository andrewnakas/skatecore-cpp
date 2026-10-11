// Port of skate-game/src/physics/player_state/pre_state.rs.
//! Update3_State82DB6050 prefix through grab-spline query synchronization.
//! Ground/Air/KnownAir vtable24 ->82D34DA8 ->82D2D860 predicts from the actual deck part
//! transform and its physical body's linear velocity, with a single VMADD.
#include "s3/game/physics/player_state/pre_state.hpp"

#include "s3/core/physics/board.hpp"
#include "s3/core/player/offboard/ground_query.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/offboard/grab_scene.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::player_state::pre_state_ {

using sc::physics::board::BodyId;
using sc::player::state::PhysicalStateId;

Status advance(GamePhysics& physics, SkaterRuntime& skater) {
    auto& player = skater.player_input.player;
    player.state_count_1312 = player.state_count_1312 + 1u;
    // KnownAir ctor82D35130 installs82327210; vslot24 at82327228 is
    // the same82D34DA8. Its target-following trajectory is not this deck sample.
    // Six grind vtables call82D2D860 directly;701 calls the82D34DA8 wrapper.
    const auto current = skater.player_state.current();
    bool known = false;
    switch (current) {
        case PhysicalStateId::PhysicsGround:
        case PhysicalStateId::PhysicsAir:
        case PhysicalStateId::PhysicsAirSecondary:
        case PhysicalStateId::FootPlant:
        case PhysicalStateId::Boneless:
        case PhysicalStateId::HandPlant:
        case PhysicalStateId::RevertGround:
        case PhysicalStateId::KnownAir:
        case PhysicalStateId::BipedAir:
        case PhysicalStateId::BipedGround:
        case PhysicalStateId::OffBoardPushing:
        case PhysicalStateId::GroundAnimation:
        case PhysicalStateId::SlideGround:
        case PhysicalStateId::WipeoutGround:
        case PhysicalStateId::Teleporting:
        case PhysicalStateId::LandingOnDeck:
        case PhysicalStateId::Nonspecific: known = true; break;
        default: break;
    }
    if (!sc::player::state::is_grind(current) && !known) {
        return rs::make_err(std::string("PreState requires the selected state's actual PredictFutureOfDeck"));
    }
    const auto deck = physics.board.part_transforms()[sc::physics::board::index(BodyId::Deck)];
    const auto velocity = physics.board.bodies()[sc::physics::board::index(BodyId::Deck)].rates.linear_velocity;
    const float dt = skater.player_input.processed.timestep_2604;
    const std::array<float, 4> prediction{
        std::fma(velocity.x, dt, deck.translation.x),
        std::fma(velocity.y, dt, deck.translation.y),
        std::fma(velocity.z, dt, deck.translation.z),
        0.0f,
    };
    auto& roots = skater.animated_skeleton.roots;
    roots.predicted_board_position = prediction;
    roots.supplied_prediction = prediction;
    // 82DB60DC clears sticky3184, not this-update3185.
    skater.foot_ik.state.contacts.support_failed = false;
    // 82DB60EC calls82D74270 AFTER sticky reset and before state PreUpdate.
    // Consume the canonical owner's preceding validation/query results.
    const auto& p = skater.player_input.processed;
    const sc::player::offboard::ground_query::QueryContext context{p.actor_query_2948,
                                                                   static_cast<std::int32_t>(p.actor_query_2952)};
    const auto scene = offboard::grab_scene::Scene::new_(physics.world, physics.offboard_grab_scene);
    S3G_CHECK(skater.offboard_grab.sync(scene, context));
    return ok();
}

}  // namespace s3::game::physics::player_state::pre_state_
