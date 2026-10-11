// Port of skate-game/src/physics/slide_state.rs.
//! PhysicsSlideGround101 production lifecycle, original TU3 vtable823285B4.
#pragma once
#include <string>
#include <utility>
#include <vector>

#include "s3/core/physics/contact.hpp"
#include "s3/core/player/slide_state.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"
#include "s3/game/physics/slide_state/settings.hpp"
#include "s3/game/physics/slide_state/update.hpp"

namespace s3::game::physics::slide_state {

using sc::physics::contact::RetailContactMaterial;
using sc::player::slide_state::SlideSettings;
using sc::player::slide_state::SlideSurface;

struct SlideState {
    sc::player::slide_state::SlideState state{};
    SlideSettings settings{};
    std::vector<std::pair<SlideSurface, RetailContactMaterial>> surfaces;
    float manual_scalar = 0.0f;

    /// slide_state/settings.rs.
    static rs::Result<SlideState, std::string> load(const sd::Collections& data);
};

/// Enter82D3A700. Body652 points to the deck's real dynamics; its+48 is angular
/// velocity. Preserve all other body rates, inertia and collision modes.
Status enter(GamePhysics& physics, SkaterRuntime& skater);
/// Exit82D3A828 restores the board's cached standard wheel material8280.
Status exit(GamePhysics& physics, SkaterRuntime& skater);
/// Update82D3A890: complete Reckoning, Skeleton Ground, then Slide board.
Status update(GamePhysics& physics, SkaterRuntime& skater);
// Post vtable+40 is the same82D387A8 as Ground. The shared coordinator invokes
// Wipeout::check_ground with the actual complete solved Observations once.

}  // namespace s3::game::physics::slide_state
