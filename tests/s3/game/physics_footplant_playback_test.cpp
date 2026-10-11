// Port of skate-game/src/tests/footplant_playback.rs (mounted in physics.rs as `footplant_tests`).
//! The ground plant family: bonelesses, fastplants, the no-comply and the hippy jump.
//!
//! All four `PushTrick` children (`Motion.OnBoard.Trick.PushTrick.*`) are entered from the
//! ground rather than from the air, and `docs/plants.md` records the controller recipes the
//! stock graphs accept. A ground grab is stricter than an air grab: `trick_intentions::produce`
//! advances the ground timers only when the trigger action reads *exactly* 1.0, so a partial
//! analog value is not a held grab.
// The Rust tests are `#[ignore = "requires private stock graphs and animation assets"]`; here they
// run against the cleanroom pack and skip without it.
#include <algorithm>
#include <cstdio>
#include <optional>
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

// The no-comply is deliberately not covered here yet. `T_PushTrick.xml`'s `PlantingFoot` wants
// two MotionGraph intents together -- `NewFootPlant` *and* a `NoComply` trick intent -- and a
// push plus an ollie scoop produces `Ollie`, not `NoComply`, however the two are spaced: the
// push enters (`B_PUSH_INTO`) and the scoop then pops an ordinary ollie straight over the top of
// it. Which gesture group `CreateTrickIntentFromGesture` is meant to resolve under a planted
// push foot is the open question; see docs/trick-input-recipes.md.

/// Xbox face buttons as `XboxState::buttons` bits: A is `RightPush`, X is `LeftPush`.
constexpr std::uint16_t A = 0x1000;
constexpr std::uint16_t X = 0x4000;

/// Tick the push arrives on, after the roll-in has settled and any grab is already held.
constexpr std::uint32_t PUSH = 150;
/// Tick the grab trigger goes down, well before the push.
constexpr std::uint32_t GRAB = 120;
/// Ticks the push is held before release. The hippy jump is a *charge*: its antic cycles for as
/// long as both pushes are down (`T_HIPPYJUMP_ANTIC_CYC`) and only launches on the release, so a
/// button held for the whole run never leaves the anticipation.
constexpr std::uint32_t PUSH_HOLD = 40;
/// Ticks after the push before the scoop. The no-comply wants the flick *during push entry*, so
/// a scoop thrown on the same tick as the button simply pops an ordinary ollie first.
constexpr std::uint32_t SCOOP_DELAY = 12;

struct Outcome {
    std::vector<std::string> animations;
    std::vector<std::string> tricks;
};

constexpr std::array<const char*, 7> WATCHED{
    "fsboneless", "bsboneless", "fsfastplant", "bsfastplant", "nocomply", "hippyjump", "ollie",
};

Outcome replay(const pb::Stock& s, std::optional<std::size_t> hand, std::uint16_t buttons,
               std::optional<std::string> scoop_name, std::uint32_t ticks) {
    auto physics = pb::unwrap(GamePhysics::load_with_terrain(*s.vfs, s.root, ground::Terrain::Course), "physics");
    auto skater = pb::unwrap(SkaterRuntime::load(*s.vfs, s.root, s.graphs, physics, "normal"), "skater");
    auto camera = pb::unwrap(camera::CameraRuntime::load(*s.vfs, s.root), "camera");
    input::controllers::ControllerInput controller{};
    auto controls = pb::unwrap(PlayerControls::load(*s.vfs, s.root), "controls");

    std::vector<std::array<float, 2>> scoop;
    if (scoop_name) {
        bool found = false;
        for (auto& p : pb::patterns(s, "skater.pat")) {
            if (p.name == *scoop_name) {
                scoop = std::move(p.points);
                found = true;
                break;
            }
        }
        if (!found) pb::panic("authored " + *scoop_name + " pattern");
    }
    std::vector<std::pair<std::string, s3::animation::output::attributes::AttributeName>> watched;
    for (const char* n : WATCHED) watched.emplace_back(n, encode(n));

    Outcome outcome{};
    for (std::uint32_t tick = 0; tick < ticks; ++tick) {
        if (tick == 60) pb::roll(physics, skater);
        // 255 is exactly 1.0 after conversion, which is what a *ground* grab requires.
        std::array<std::uint8_t, 2> triggers{0, 0};
        if (hand && tick >= GRAB) triggers[*hand] = 255;
        // The scoop rides on top of the push, during its entry.
        std::array<std::int16_t, 2> right{0, 0};
        if (!(scoop.empty() || tick < PUSH + SCOOP_DELAY)) {
            const std::size_t step = std::size_t(tick - PUSH - SCOOP_DELAY) / 3;
            if (step < scoop.size()) right = pb::sample(scoop[step]);
        }
        const std::uint16_t held = (tick >= PUSH && tick < PUSH + PUSH_HOLD) ? buttons : 0;
        const auto result =
            pb::tick(physics, skater, controls, controller, camera, s, pb::xbox(held, triggers, {0, 0}, right), true);
        if (!result.is_ok()) pb::panic("tick" + std::to_string(tick) + ": " + result.unwrap_err());

        const auto& motion = skater.animation.motion;
        if (motion.animation.current_name &&
            (outcome.animations.empty() || outcome.animations.back() != *motion.animation.current_name)) {
            outcome.animations.push_back(*motion.animation.current_name);
        }
        if (const auto published = motion.score_packet.trick_names.first) {
            const auto found = std::find_if(watched.begin(), watched.end(),
                                            [&](const auto& w) { return w.second == *published; });
            if (found != watched.end() && (outcome.tricks.empty() || outcome.tricks.back() != found->first)) {
                outcome.tricks.push_back(found->first);
            }
        }
    }
    char buttons_text[16];
    std::snprintf(buttons_text, sizeof buttons_text, "%#06x", buttons);
    std::fprintf(stderr, "hand=%s buttons=%s: tricks=%s\n  animations=%s\n",
                 hand ? ("Some(" + std::to_string(*hand) + ")").c_str() : "None", buttons_text,
                 pb::debug_strings(outcome.tricks).c_str(), pb::debug_strings(outcome.animations).c_str());
    return outcome;
}

}  // namespace

/// Hold a grab trigger fully, then press a push button. Which trigger and which push foot select
/// the stock variant; `docs/plants.md` has the mapping.
// #[ignore = "requires private stock graphs and animation assets"]
S3_TEST(held_ground_grab_plus_a_push_reaches_the_plant_family) {
    const auto* s = pb::stock();
    if (s == nullptr) return;
    std::vector<std::string> seen;
    for (std::size_t hand = 0; hand < 2; ++hand) {
        for (const std::uint16_t button : {A, X}) {
            auto run = replay(*s, hand, button, std::nullopt, 300);
            std::fprintf(stderr, "hand=%zu button=%#06x -> %s\n", hand, button, pb::debug_strings(run.tricks).c_str());
            seen.insert(seen.end(), run.tricks.begin(), run.tricks.end());
        }
    }
    for (const char* expected : {"fsboneless", "bsboneless", "fsfastplant", "bsfastplant"}) {
        CHECK_MSG(std::find(seen.begin(), seen.end(), expected) != seen.end(),
                  expected << " was never published across the four trigger/push combinations; saw "
                           << pb::debug_strings(seen));
    }
}

/// Both pushes at once, which `ActionGraphIncludes/onground.xml`'s `HippyJumping` is gated on.
// #[ignore = "requires private stock graphs and animation assets"]
S3_TEST(both_pushes_reach_the_hippy_jump) {
    const auto* s = pb::stock();
    if (s == nullptr) return;
    const auto run = replay(*s, std::nullopt, A | X, std::nullopt, 300);
    CHECK_MSG(std::find(run.tricks.begin(), run.tricks.end(), "hippyjump") != run.tricks.end(),
              "hippyjump was never published; saw " << pb::debug_strings(run.tricks) << " over "
                                                    << pb::debug_strings(run.animations));
}
