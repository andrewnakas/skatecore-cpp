// Port of skate-game/src/physics/footplant/update.rs.
//! Original82D6FD60 ->82D6F5F0 ->82D6F910, invoked by82D36728.
//! (`impl Footplant` update methods are declared in footplant.hpp.)
#pragma once
#include <array>

#include "s3/core/physics/board_toolkit.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/grind_contact.hpp"
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/physics/slice.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::animated_skeleton {
struct AnimatedSkeleton;  // physics/animated_skeleton.hpp (group D)
}

namespace s3::game::physics::footplant::update {

using V = std::array<float, 4>;

struct FootplantFrame {
    const sc::player::input_phase::ProcessedPhysicsInput& processed;
    const sc::physics::board_toolkit::BoardToolkit& toolkit;
    const animated_skeleton::AnimatedSkeleton& animated;
    const sc::physics::skeleton_body::SkeletonBody& body;
    V gravity{};
    const sc::physics::board_world::BoardWorld& world;
    /// Rust `&'a [Primitive]`.
    sc::physics::Slice<const sc::physics::grind_contact::Primitive> edges;
};

}  // namespace s3::game::physics::footplant::update
