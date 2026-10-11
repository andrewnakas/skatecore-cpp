// Port of skate-game/src/physics/offboard/landing_deck/output.rs
//! Complete82D79420 writes into caller-owned physical output fields.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/player/input_phase.hpp"
#include "s3/game/physics/offboard/landing_deck.hpp"

namespace s3::game::physics::offboard::landing_deck {

///Borrow actual shared output storage; this type retains no parallel state.
///The parent must supply native skeleton+48/+3482 destinations, not temporaries.
struct PublishTargets {
    sc::player::input_phase::OffBoardOutputFields* off_board = nullptr;
    std::array<std::uint32_t, 4>* skeleton_position_48 = nullptr;
    std::uint8_t* skeleton_flag_3482 = nullptr;
};

namespace output {
using landing_deck::PublishTargets;
}

}  // namespace s3::game::physics::offboard::landing_deck
