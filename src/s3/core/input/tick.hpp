// Port of skate-core/src/input/tick.rs.
// Immutable fixed-tick input boundary.
//
// The platform adapter owns device polling. Once a `TickInput` exists, all
// simulation phases consume this value; they do not access a platform device,
// pad history, or rebuild the gameplay map.
#pragma once
#include <cstdint>

#include "s3/core/input/gameplay_map.hpp"

namespace s3::input::tick {

using gameplay_map::GameplayActions;

class TickInput {
public:
    static TickInput new_(std::uint64_t tick, GameplayActions actions, bool controller_available) {
        TickInput t;
        t.tick_ = tick;
        t.actions_ = actions;
        t.controller_available_ = controller_available;
        return t;
    }
    std::uint64_t tick() const { return tick_; }
    GameplayActions actions() const { return actions_; }
    bool controller_available() const { return controller_available_; }

    bool operator==(const TickInput& o) const {
        return tick_ == o.tick_ && actions_ == o.actions_ && controller_available_ == o.controller_available_;
    }
    bool operator!=(const TickInput& o) const { return !(*this == o); }

private:
    std::uint64_t tick_ = 0;
    GameplayActions actions_;
    bool controller_available_ = false;
};

}  // namespace s3::input::tick
