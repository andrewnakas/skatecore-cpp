// Port of skate-core/src/physics/filtered_state.rs.
#include "s3/core/physics/filtered_state.hpp"

namespace s3::physics::filtered_state {

namespace {
std::int32_t count(std::int32_t previous, bool active) {
    return active ? static_cast<std::int32_t>(static_cast<std::uint32_t>(previous) + 1u) : 0;
}
}  // namespace

FilteredStateOutput FilteredState::update(FilteredStateInput input) {
    using C = FilteredCategory;
    const C old = category;
    previous_category = old;
    must_change = old == C::Wipeout || old == C::Invalid ||
                  (previous_physics_state == 702 && input.physics_state != 702);
    air_count = count(air_count, input.physics_category == 200);
    nonspecific_count = count(nonspecific_count, input.physics_state == 701);
    nonspecific_collision_free_count = count(
        nonspecific_collision_free_count, input.physics_state == 701 && !input.anything_in_contact);
    nonspecific_collision_count = count(nonspecific_collision_count,
                                        input.physics_state == 701 && input.anything_in_contact);
    frames_since_ground_stairs =
        count(frames_since_ground_stairs,
              !(input.physics_category == 100 && input.physics_surface_type == 8));
    const std::int32_t s = input.physics_state;
    if (s == 100) {
        category = input.wall_ride_exit ? C::Air : C::Ground;
    } else if ((s >= 101 && s <= 105) || s == 602) {
        category = C::Ground;
    } else if (s == 200) {
        const bool air_delay_passed = air_count > 3;
        if (old == C::Air) {
            // TU3 adds contact return to ground, absent in the named S2 body.
            category = ((air_delay_passed && input.anything_in_contact) || must_change) ? C::Ground
                                                                                         : C::Air;
        } else {
            const bool may_enter_air =
                air_delay_passed && (old != C::Ground || frames_since_ground_stairs > 9);
            category = (may_enter_air || must_change) ? C::Air : old;
        }
    } else if (s == 201) {
        category = (input.anything_in_contact && !input.targeting_grind && air_count > 5)
                       ? C::Ground
                       : C::Air;
    } else if (s == 202 || s == 600 || s == 601) {
        category = C::Air;
    } else if (s == 300) {
        category = C::Wipeout;
    } else if (s >= 400 && s <= 405) {
        cached_grind = input.grind;
        category = C::Grind;
    } else if (s == 500 || s == 502) {
        category = C::Offboard;
    } else if (s == 501) {
        category = input.offboard_has_landed ? C::Offboard : C::OffboardAir;
    } else if (s == 503) {
        category = input.offboard_on_deck ? C::Ground : C::Offboard;
    } else if (s == 702) {
        category = C::Teleport;
    } else {
        // Native701 and unrecognized states retain the prior category.
        category = old;
    }
    previous_physics_state = input.physics_state;
    const bool grinding = category == C::Grind;
    return FilteredStateOutput{
        category,
        previous_category,
        grinding,
        grinding ? cached_grind : GrindState::default_(),
        input.last_grind_distance,
    };
}

}  // namespace s3::physics::filtered_state
