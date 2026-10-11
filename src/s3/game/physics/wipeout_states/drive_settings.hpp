// Port of skate-game/src/physics/wipeout_states/drive_settings.rs
//! Native82BE7860's per-part records and82BEB0C8/82BEB790 settings.
#pragma once
#include <string>

#include "s3/core/player/wipeout_state.hpp"
#include "s3/data/collections.hpp"
#include "s3/data/vfs.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::wipeout_states::drive_settings {

rs::Result<::s3::player::wipeout_state::drives::Settings, std::string> load(const sd::Collections& data,
                                                                           const sd::Vfs& vfs,
                                                                           const std::string& asset_root,
                                                                           const std::string& primary_bank_sha);

}  // namespace s3::game::physics::wipeout_states::drive_settings
