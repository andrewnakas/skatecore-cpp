// Port of skate-game/src/physics/wipeout_states/mod.rs
//! Original physical WipeoutGround300. Off-board recovery destinations are
//! separate states; this owner retains the native request and timing fields.
#pragma once
#include <array>
#include <string>

#include "s3/core/player/wipeout_state.hpp"
#include "s3/data/collections.hpp"
#include "s3/data/vfs.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/wipeout_states/contact.hpp"
#include "s3/game/physics/wipeout_states/drive_settings.hpp"
#include "s3/game/physics/wipeout_states/lifecycle.hpp"
#include "s3/game/physics/wipeout_states/output.hpp"
#include "s3/game/physics/wipeout_states/prediction.hpp"
#include "s3/game/physics/wipeout_states/ragdoll.hpp"
#include "s3/game/physics/wipeout_states/settings.hpp"
#include "s3/game/physics/wipeout_states/skeleton.hpp"
#include "s3/game/physics/wipeout_states/update.hpp"

namespace s3::game::physics::wipeout_states {

struct WipeoutState {
    ragdoll::RagdollSetup ragdoll{};
    ::s3::player::wipeout_state::State state{};
    // Private in Rust (accessed by the child modules); public here.
    settings::Settings settings{};
    ::s3::player::wipeout_state::drives::Settings drives{};
    std::array<::s3::player::wipeout_state::profiles::Profile, 5> profiles{};
    ::s3::player::wipeout_state::contact_response::ContactResponse contact =
        ::s3::player::wipeout_state::contact_response::ContactResponse::new_();
    prediction::Prediction prediction = prediction::Prediction::new_();

    /// `assets: &Path` -> (vfs, asset root).
    static rs::Result<WipeoutState, std::string> load(const sd::Collections& data, const sd::Vfs& vfs,
                                                      const std::string& assets,
                                                      const std::string& primary_bank_sha);
};

// pub(crate) use lifecycle::{enter, exit, post_physics}; output::fill; update::advance;
using lifecycle::enter;
using lifecycle::exit;
using lifecycle::post_physics;
using output::fill;
using update::advance;

}  // namespace s3::game::physics::wipeout_states
