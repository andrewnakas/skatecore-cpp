// Port of skate-game/src/physics/player_state/registry.rs `mod tests`.
#include <vector>

#include "harness.hpp"
#include "s3/game/physics/player_state/registry.hpp"

using namespace s3::game::physics::player_state::registry;
namespace state = s3::player::state;

S3_TEST(registry_exposes_connected_biped_state_lifecycles) {
    const auto registry = StateRegistry::new_();
    CHECK(registry.capability(PhysicalStateId::PhysicsGround).supported);
    CHECK(registry.can_transition(PhysicalStateId::PhysicsGround, PhysicalStateId::PhysicsAir));
    CHECK(!registry.can_transition(PhysicalStateId::Sleeping, PhysicalStateId::PhysicsAir));
    CHECK(registry.capability(PhysicalStateId::BipedAir).supported);
}

/// The states whose native adapters are still missing, and what each one costs.
///
/// This list is deliberately exhaustive rather than a `!supported` filter: an unported
/// state is only discovered when the selector asks for it mid-play, and `transition::set`
/// turns that into a fatal error. Pinning the set here makes adding or removing one a
/// reviewed edit instead of a crash somebody hits in a playtest.
///
/// * `Skitching` (104) -- holding onto a vehicle.
/// * `FollowPath` (105) -- scripted/living-world movement.
S3_TEST(the_unported_states_are_exactly_the_documented_two) {
    const auto registry = StateRegistry::new_();
    std::vector<PhysicalStateId> unported;
    for (const auto id : state::ALL)
        if (!registry.capability(id).supported) unported.push_back(id);
    CHECK_MSG((unported == std::vector<PhysicalStateId>{PhysicalStateId::Skitching, PhysicalStateId::FollowPath}),
              "unported state set changed; update this list and docs/engine-defects.md");
}
