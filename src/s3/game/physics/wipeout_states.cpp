// Port of skate-game/src/physics/wipeout_states/mod.rs
//! Original physical WipeoutGround300. Off-board recovery destinations are
//! separate states; this owner retains the native request and timing fields.
#include "s3/game/physics/wipeout_states.hpp"

namespace s3::game::physics::wipeout_states {

using ::s3::player::wipeout_state::profiles::Profile;

rs::Result<WipeoutState, std::string> WipeoutState::load(const sd::Collections& data, const sd::Vfs& vfs,
                                                         const std::string& assets,
                                                         const std::string& primary_bank_sha) {
    //Ctor82D3B1EC..B25C full64 handle hashes, verified against the stock keys.
    const std::array<const char*, 5> names{"free_fall", "cannon_ball", "judo_kick", "swan_dive", "torpedo"};
    std::vector<rs::Result<Profile, std::string>> profiles;
    profiles.reserve(5);
    for (const char* name : names) profiles.push_back(settings::load_profile(data, name));
    WipeoutState w;
    S3G_TRY(w.ragdoll, ragdoll::RagdollSetup::load(data, vfs, assets, primary_bank_sha));
    w.state = ::s3::player::wipeout_state::State{};
    S3G_TRY(w.settings, settings::Settings::load(data));
    S3G_TRY(w.drives, drive_settings::load(data, vfs, assets, primary_bank_sha));
    for (std::size_t i = 0; i < 5; ++i) {
        S3G_TRY(w.profiles[i], profiles[i]);
    }
    w.contact = ::s3::player::wipeout_state::contact_response::ContactResponse::new_();
    w.prediction = prediction::Prediction::new_();
    return w;
}

}  // namespace s3::game::physics::wipeout_states
