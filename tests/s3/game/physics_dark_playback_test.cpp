// Port of skate-game/src/tests/dark_playback.rs (mounted in physics.rs as `dark_tests`).
//! Dark catches: an ordinary flip scoop thrown with the dark-catch button held.
//!
//! `trick_intentions::produce` reads the request from the packed action flags rather than from
//! gesture timing -- `dark_flags = (1 << 20) | (1 << 28)`, so either of two buttons arms it --
//! and publishes `DarkCatch` while held plus `NewDarkCatch` on the rising edge.
//! `gesture_mapping::select` then resolves the flick through the dark-catch column of the
//! 270-row table, and `motion_tricks::Operation::MonitorUnderflip` latches
//! `trick_requests.dark_catch` when no `U_*` underflip intent is present instead.
// The Rust test is `#[ignore = "requires private stock graphs and animation assets"]`; here it runs
// against the cleanroom pack and skips without it.
#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "physics_playback_support.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"

using namespace s3::game;
using namespace s3::game::physics;
using s3::animation::skeleton_input::name::encode;
namespace pb = s3test::game::playback;

namespace {

constexpr std::uint16_t B = 0x2000;
constexpr std::uint16_t RB = 0x0200;

/// Takeoff boost, as in `flip_playback.rs`: the flat course's own pop is too short for the
/// catch to resolve before landing.
constexpr float BOOST = 12.0f;

struct Outcome {
    std::vector<std::string> animations;
    std::vector<std::string> tricks;
};

constexpr std::array<const char*, 8> WATCHED{
    "kickflip",           "heelflip",           "kickflip_darkcatch",  "heelflip_darkcatch",
    "kickflip2_darkcatch", "heelflip2_darkcatch", "360flip_darkcatch", "laserflip_darkcatch",
};

Outcome replay(const pb::Stock& s, const std::string& gesture, std::uint16_t buttons, std::uint32_t ticks) {
    auto physics = pb::unwrap(GamePhysics::load_with_terrain(*s.vfs, s.root, ground::Terrain::Course), "physics");
    auto skater = pb::unwrap(SkaterRuntime::load(*s.vfs, s.root, s.graphs, physics, "normal"), "skater");
    auto camera = pb::unwrap(camera::CameraRuntime::load(*s.vfs, s.root), "camera");
    input::controllers::ControllerInput controller{};
    auto controls = pb::unwrap(PlayerControls::load(*s.vfs, s.root), "controls");

    std::vector<std::array<float, 2>> scoop;
    bool found = false;
    for (auto& p : pb::patterns(s, "skater.pat")) {
        if (p.name == gesture) {
            scoop = std::move(p.points);
            found = true;
            break;
        }
    }
    if (!found) pb::panic("authored " + gesture + " pattern");
    std::vector<std::pair<std::string, s3::animation::output::attributes::AttributeName>> watched;
    for (const char* n : WATCHED) watched.emplace_back(n, encode(n));

    Outcome outcome{};
    for (std::uint32_t tick = 0; tick < ticks; ++tick) {
        if (tick == 60) pb::roll(physics, skater);
        if (tick == 123) pb::boost(physics, skater, BOOST);
        // Crouch on the scoop's first coordinate, then flick it: the ordinary ground pop.
        std::array<std::int16_t, 2> right{0, 0};
        if (tick >= 100 && tick < 112) {
            right = pb::sample(scoop[0]);
        } else if (tick >= 112 && tick < 118) {
            right = pb::sample(scoop[scoop.size() - 1]);
        }
        // The dark button is armed before the pop and held through the air, so both `DarkCatch`
        // and its rising edge are available whenever the graph looks.
        const auto result = pb::tick(physics, skater, controls, controller, camera, s,
                                     pb::xbox(tick >= 95 ? buttons : 0, {0, 0}, {0, 0}, right), true);
        if (!result.is_ok()) pb::panic(gesture + " tick" + std::to_string(tick) + ": " + result.unwrap_err());

        const auto& motion = skater.animation.motion;
        if (motion.animation.current_name &&
            (outcome.animations.empty() || outcome.animations.back() != *motion.animation.current_name)) {
            outcome.animations.push_back(*motion.animation.current_name);
        }
        if (const auto published = motion.score_packet.trick_names.first) {
            const auto it = std::find_if(watched.begin(), watched.end(),
                                         [&](const auto& w) { return w.second == *published; });
            if (it != watched.end() && (outcome.tricks.empty() || outcome.tricks.back() != it->first)) {
                outcome.tricks.push_back(it->first);
            }
        }
    }
    std::fprintf(stderr, "%s buttons=%#06x: tricks=%s\n  animations=%s\n", gesture.c_str(), buttons,
                 pb::debug_strings(outcome.tricks).c_str(), pb::debug_strings(outcome.animations).c_str());
    return outcome;
}

}  // namespace

/// The kickflip and heelflip dark catches, off the same authored scoops the ordinary ladder
/// uses.
///
/// `dark_flags` arms on either of two packed bits, but only RB reaches the catch from a rolling
/// approach: the other bit is B, and B while riding is `Brake` (`riding_intentions` emits it
/// from the same button), so the pop never happens. Both are asserted so that stays pinned.
// #[ignore = "requires private stock graphs and animation assets"]
S3_TEST(a_flip_with_the_dark_button_held_reaches_the_dark_catch) {
    const auto* s = pb::stock();
    if (s == nullptr) return;
    for (const auto& [gesture, trick] : {std::pair<std::string, std::string>{"Kickflip", "kickflip_darkcatch"},
                                         {"Heelflip", "heelflip_darkcatch"}}) {
        const auto armed = replay(*s, gesture, RB, 300);
        CHECK_MSG(std::find(armed.tricks.begin(), armed.tricks.end(), trick) != armed.tricks.end(),
                  trick << ": RB never published it; saw " << pb::debug_strings(armed.tricks) << " over "
                        << pb::debug_strings(armed.animations));
        const auto braked = replay(*s, gesture, B, 300);
        CHECK_MSG(braked.tricks.empty(), gesture << ": B is Brake while riding, so it should not pop at all; saw "
                                                 << pb::debug_strings(braked.tricks));
    }
}
