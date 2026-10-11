// Port of skate-game/src/physics/player_input/grind.rs.
//! Host manager82D8A828/82D8AB08, using CURRENT player-input ownership.
//! Static authored-world scope. Gameplay leaves live in skate-core; world
//! filtering, primitive provenance and board material application stay explicit.
#include "s3/game/physics/player_input/grind.hpp"

namespace s3::game::physics::player_input::grind {

rs::Result<GrindInputState, std::string> GrindInputState::load(const sd::Collections& data) {
    S3G_TRY(auto loaded_settings, settings::Settings::load(data));
    GrindInputState s;
    s.previous_state = 0;
    s.engagement_counter = 0;
    s.cooldown = 0;
    s.disabled = false;
    s.suppressed = false;
    s.elapsed = 0.0f;
    s.friction_vs_time = 0.0f;
    s.previous_velocity = {0, 0, 0, 0};
    s.grind_history = 0;
    s.secondary_history = 0;
    s.grounded_frames = 0;
    s.air_frames = 0;
    s.low_wheel_frames = 0;
    s.previous_proximity = false;
    s.previous_air_target = false;
    s.previous_direction = {0.0f, 0.0f, 0.0f, 0.0f};
    s.gravity_timer = 0.0f;
    s.balance = balance::BalanceState::default_();
    s.engagement = entry::Engagement{};
    s.control = control::Control{};
    s.jumper = manager::Jumper::default_();
    s.investigation = GrindInvestigationFields{};
    s.settings = std::move(loaded_settings);
    return s;
}

void GrindInputState::reset() {
    previous_state = 0;
    engagement_counter = 0;
    cooldown = 0;
    disabled = false;
    suppressed = false;
    elapsed = 0.0f;
    gravity_timer = 0.0f;
    friction_vs_time = 0.0f;
    previous_direction = {0.0f, 0.0f, 0.0f, 0.0f};
    investigation = GrindInvestigationFields{};
}

V float_(std::array<std::uint32_t, 4> v) { return rs::from_bits(v); }

std::array<std::uint32_t, 4> raw(V v) { return rs::to_bits(v); }

}  // namespace s3::game::physics::player_input::grind
