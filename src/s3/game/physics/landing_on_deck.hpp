// Port of skate-game/src/physics/landing_on_deck.rs
//! State503 lifecycle from original TU3 82D4D418..82D4E100.
#pragma once
#include <string>

#include "s3/core/physics/skeleton_landing_on_board.hpp"
#include "s3/core/player/offboard/landing_state.hpp"
#include "s3/core/player/wipeout/data.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"
#include "s3/game/physics/landing_on_deck/publication.hpp"
#include "s3/game/physics/landing_on_deck/skeleton.hpp"

namespace s3::game::physics::landing_on_deck {

// pub(crate) use publication::fill;
using publication::fill;

using RootSettings = ::s3::physics::skeleton_landing_on_board::Settings;
using ::s3::player::offboard::landing_state::Entry;
using ::s3::player::offboard::landing_state::Settings;
using ::s3::player::offboard::landing_state::State;
using ::s3::player::wipeout::Frame;

struct Runtime {
    State state{};
    Settings settings{};           // private in Rust
    RootSettings root_settings{};  // private in Rust

    static rs::Result<Runtime, std::string> load(const sd::Collections& data);
};

void clear_ik(SkaterRuntime& skater);
Status enter(GamePhysics& physics, SkaterRuntime& skater);
Status advance(GamePhysics& physics, SkaterRuntime& skater);
///82D4DF08: synchronize actual query completion before rejection/collision tests.
Status post(SkaterRuntime& skater, Frame frame);
///82D4D830 resets only the shared manager; outgoing state data is not another trajectory.
Status exit(GamePhysics& physics, SkaterRuntime& skater);

}  // namespace s3::game::physics::landing_on_deck
