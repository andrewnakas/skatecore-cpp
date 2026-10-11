// Port of skate-game/src/physics/player_state/registry.rs.
//! Coordinator-owned physical state registry.
//!
//! TU3 has one lifecycle object per PhysicalPlayer state. Keeping the
//! support table here makes selection, transition, and diagnostics agree on
//! which owners are actually connected. Task-B-owned states remain explicit
//! unsupported entries until their native adapters are integrated.
#include "s3/game/physics/player_state/registry.hpp"

namespace s3::game::physics::player_state::registry {

namespace {
/// The ordinary (non-grind, non-Nonspecific) connected states, shared by both tables.
bool ordinary(PhysicalStateId id) {
    switch (id) {
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
        case PhysicalStateId::LandingOnDeck: return true;
        default: return false;
    }
}
}  // namespace

StateCapability StateRegistry::capability(PhysicalStateId id) const {
    const bool supported = sc::player::state::is_grind(id) || id == PhysicalStateId::Nonspecific ||
                           id == PhysicalStateId::Sleeping || ordinary(id);
    return StateCapability{id, supported, supported && id != PhysicalStateId::Sleeping, supported};
}

bool StateRegistry::can_transition(PhysicalStateId current, PhysicalStateId requested) const {
    if (!capability(current).supported || !capability(requested).supported) return false;
    if (current != PhysicalStateId::Sleeping &&
        (sc::player::state::is_grind(current) || sc::player::state::is_grind(requested) ||
         current == PhysicalStateId::Nonspecific || requested == PhysicalStateId::Nonspecific)) {
        return true;
    }
    return (current == PhysicalStateId::Sleeping && requested == PhysicalStateId::PhysicsGround) ||
           (ordinary(current) && ordinary(requested));
}

}  // namespace s3::game::physics::player_state::registry
