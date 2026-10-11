// Port of skate-game/src/physics/biped_ground/grab.rs
//! Concrete Ground grab decision82D324B0. Query readiness belongs to ONE owner.
#pragma once
#include <cstdint>

#include "s3/core/player/offboard/grab_scene.hpp"
#include "s3/core/player/offboard/ground_entry.hpp"
#include "s3/core/player/offboard/ground_query.hpp"
#include "s3/core/player/offboard/ground_sync.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/biped_ground/grab_runtime.hpp"

namespace s3::game::physics::biped_ground::grab {

using grab_runtime::Owner;
using ::s3::player::offboard::ground_entry::Frame;
using ::s3::player::offboard::ground_entry::State;
using ::s3::player::offboard::ground_entry::Vector;
using ::s3::player::offboard::ground_query::QueryContext;
using ::s3::player::offboard::ground_sync::BoardLimits;
using ::s3::player::offboard::ground_sync::BoardSettings;

struct Input {
    Frame frame{};
    Vector position{};
    Vector bone{};
    std::uint32_t flags_2476 = 0;
    std::uint32_t flags_2480 = 0;
    std::uint32_t flags_2484 = 0;
    float elapsed = 0.0f;
    float timer_2852 = 0.0f;
    QueryContext context{};
};

void sync(State& state, Owner& owner, BoardSettings settings, Input p);

BoardLimits limits(float margin, float a, float b);

}  // namespace s3::game::physics::biped_ground::grab
