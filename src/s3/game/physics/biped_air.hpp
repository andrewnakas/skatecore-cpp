// Port of skate-game/src/physics/biped_air.rs
//! Distinct State501 lifecycle: original S3 82D2E5C0/EF38/FFF8/30300/EF20.
#pragma once
#include <string>

#include "s3/core/player/offboard/biped_air/post.hpp"
#include "s3/core/player/offboard/biped_air/recovered.hpp"
#include "s3/core/player/offboard/controller.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/biped_air/input.hpp"
#include "s3/game/physics/biped_air/post.hpp"
#include "s3/game/physics/biped_air/publication.hpp"
#include "s3/game/physics/biped_air/settings.hpp"
#include "s3/game/physics/biped_air/update.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::biped_air {

// pub(crate) use post::post; pub(crate) use update::update;
// NOT re-exported: a using-declaration `update`/`post` would collide with the namespaces of the same
// name (ambiguous lookup). Callers spell `biped_air::update::update(...)` and `biped_air::post::post(...)`.

struct BipedAir {
    ::s3::player::offboard::biped_air::recovered::State state{};
    ::s3::player::offboard::biped_air::recovered::post::Settings checks{};  // private in Rust

    static rs::Result<BipedAir, std::string> load(const sd::Collections& data);
};

///82DB3C78 at the early fixed-step phase, before current ProcessInput.
///82859E70 completes earlier trajectory work atA06C/A080 and invokes virtual
///slot16 atA0D0. Actual vtable82328688+16 points to82DB3C78.
Status consume_selector(const GamePhysics& physics, SkaterRuntime& skater);
Status enter(const GamePhysics& physics, SkaterRuntime& skater);
void exit(SkaterRuntime& skater);
Status fill(SkaterRuntime& skater);

}  // namespace s3::game::physics::biped_air
