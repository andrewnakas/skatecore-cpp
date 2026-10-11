// Port of skate-game/src/physics/player_input/grind/post.rs.
//! (`impl GrindInputState::post_update` is declared in grind.hpp.)
#pragma once
#include <cstdint>
#include <string>

#include "s3/core/physics/grind_contact/admission.hpp"
#include "s3/game/physics/player_input/grind.hpp"

namespace s3::game::physics::player_input::grind::post {

/// Private in Rust.
rs::Result<sc::physics::grind_contact::admission::EntryKind, std::string> entry_kind(std::uint32_t value);

}  // namespace s3::game::physics::player_input::grind::post
