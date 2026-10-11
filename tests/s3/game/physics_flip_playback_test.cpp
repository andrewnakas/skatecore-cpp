// Port of skate-game/src/tests/flip_playback.rs (mounted in physics.rs as `flip_tests`).
//! The stock held-flip ladder and the air late flips.
//!
//! `T_Kickflip.xml` turns one authored flip into `<Trick>2`/`3`/`4` by cycling
//! `B_<TRICK>_CYC1..3`, but only while the scoop coordinate is still held *and*
//! the takeoff trajectory has the authored time left: `End.Out.Out1` claims the
//! exit at `TimeToLand < 0.525` and `Out2` at `< 0.8`, and those exits are
//! listed before the cycle transitions, so a short pop ends the ladder early.
//! `KnownAir` takes its prediction once, on entering the air, so the pop alone
//! decides how far the ladder can run. That is why these replays pop hard
//! instead of editing the world: the canonical course carries authored query
//! metadata that a rebuilt `BoardWorld` would drop.
//!
//! Every input here is a real controller sample fed through the production
//! gesture recognizer, action graph, motion graph and physics.
// The Rust tests are `#[ignore = "requires private stock graphs and animation assets"]`; here they
// run against the cleanroom pack and skip without it.
//
// Port note: the Rust reads `skater.scoring.hud_input().trick_name`, which the SIMREF extraction
// removed (scoring_runtime.rs: "HUD coupling ... Runtime::hud_input removed"); the composed name it
// carried is `Runtime::displayed_name()` (the same `trick_name` field).
#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "physics_playback_support.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/core/player/state.hpp"

using namespace s3::game;
using namespace s3::game::physics;
using s3::animation::skeleton_input::name::encode;
namespace pb = s3test::game::playback;

namespace {

struct Input {
    enum class Kind {
        /// Hold the ground scoop's final coordinate through the whole air.
        Held,
        /// Flick and release, so the ladder ends at the single flip.
        Single,
        /// Ollie, then run a `skater_air.pat` scoop once airborne.
        Late,
    } kind;
    std::string trick;
    float boost = 0.0f;
    std::string gesture;

    static Input held(std::string trick, float boost) { return {Kind::Held, std::move(trick), boost, {}}; }
    static Input single(std::string trick, float boost) { return {Kind::Single, std::move(trick), boost, {}}; }
    static Input late(std::string gesture, std::string trick) {
        return {Kind::Late, std::move(trick), 0.0f, std::move(gesture)};
    }
};

struct Outcome {
    std::vector<std::string> animations;
    std::vector<std::string> tricks;
    float reward = 0.0f;
    std::string name;
};

bool contains(const std::vector<std::string>& v, const std::string& s) {
    return std::find(v.begin(), v.end(), s) != v.end();
}

bool ends_with(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::vector<std::array<float, 2>> authored(const char* file, const pb::Stock& s, const std::string& name) {
    for (auto& p : pb::patterns(s, file)) {
        if (p.name == name) return std::move(p.points);
    }
    pb::panic("authored " + name + " pattern in " + file);
}

Outcome replay(const pb::Stock& s, const Input& input, std::uint32_t ticks) {
    auto physics = pb::unwrap(GamePhysics::load_with_terrain(*s.vfs, s.root, ground::Terrain::Course), "physics");
    auto skater = pb::unwrap(SkaterRuntime::load(*s.vfs, s.root, s.graphs, physics, "normal"), "skater");
    auto camera = pb::unwrap(camera::CameraRuntime::load(*s.vfs, s.root), "camera");
    input::controllers::ControllerInput controller{};
    auto controls = pb::unwrap(PlayerControls::load(*s.vfs, s.root), "controls");

    const bool hold = input.kind != Input::Kind::Single;
    // The authored scoops, straight out of the PATs the recognizer itself loads.
    std::vector<std::array<float, 2>> ground_scoop;
    std::vector<std::array<float, 2>> air_scoop;
    float boost = 0.0f;
    std::vector<std::string> watched_names;
    if (input.kind == Input::Kind::Late) {
        ground_scoop = authored("skater.pat", s, "Ollie");
        air_scoop = authored("skater_air.pat", s, input.gesture);
        boost = 9.0f;
        watched_names = {input.trick};
    } else {
        ground_scoop = authored("skater.pat", s, input.trick);
        boost = input.boost;
        for (const char* count : {"", "2", "3", "4"}) watched_names.push_back(input.trick + count);
    }
    std::vector<std::pair<std::string, s3::animation::output::attributes::AttributeName>> watched;
    for (auto& name : watched_names) {
        const auto encoded = encode(name);
        watched.emplace_back(name, encoded);
    }

    Outcome outcome{};
    std::uint32_t air_ticks = 0;
    for (std::uint32_t tick = 0; tick < ticks; ++tick) {
        if (tick == 60) {
            // A rolling approach, without a steering input that would also spin.
            pb::roll(physics, skater);
        }
        if (tick == 123) pb::boost(physics, skater, boost);
        std::array<std::int16_t, 2> right{0, 0};
        if (air_scoop.empty()) {
            // Crouch on the first coordinate, then hold the flick: holding the
            // final coordinate is what keeps `<Trick>Hold` published.
            if (tick >= 100 && tick < 112) {
                right = pb::sample(ground_scoop[0]);
            } else if (tick >= 112 && (hold || tick < 118)) {
                right = pb::sample(ground_scoop[ground_scoop.size() - 1]);
            }
        } else if (tick >= 100 && tick < 112) {
            right = pb::sample(ground_scoop[0]);
        } else if (tick >= 112 && tick < 118) {
            right = pb::sample(ground_scoop[ground_scoop.size() - 1]);
        } else if (air_ticks >= 10) {
            // One coordinate every four ticks, comfortably inside the authored
            // culling window, and then release so the trick can end.
            const std::size_t step = (std::size_t(air_ticks) - 10) / 4;
            if (step < air_scoop.size()) right = pb::sample(air_scoop[step]);
        }
        const auto result =
            pb::tick(physics, skater, controls, controller, camera, s, pb::xbox(0, {0, 0}, {0, 0}, right), true);
        if (!result.is_ok()) pb::panic("called `Result::unwrap()` on an `Err` value: " + result.unwrap_err());
        if (s3::player::state::category(skater.player_state.current()) == 200) air_ticks += 1;

        const auto& motion = skater.animation.motion;
        if (motion.animation.current_name) {
            const auto& name = *motion.animation.current_name;
            if (outcome.animations.empty() || outcome.animations.back() != name) outcome.animations.push_back(name);
        }
        if (const auto published = motion.score_packet.trick_names.first) {
            const auto found = std::find_if(watched.begin(), watched.end(),
                                            [&](const auto& w) { return w.second == *published; });
            if (found != watched.end()) {
                if (outcome.tricks.empty() || outcome.tricks.back() != found->first) {
                    outcome.tricks.push_back(found->first);
                }
            }
        }
        outcome.reward = skater.scoring.session.holder.snapshot.last_reward;
        const std::string published = skater.scoring.displayed_name();
        if (!published.empty() && published != "#ID_TRICK_AIR") outcome.name = published;
    }
    std::fprintf(stderr, "%s: tricks=%s reward=%s name=\"%s\"\n  animations=%s\n",
                 pb::debug_strings(watched_names).c_str(), pb::debug_strings(outcome.tricks).c_str(),
                 fmt::display_f32(outcome.reward).c_str(), outcome.name.c_str(),
                 pb::debug_strings(outcome.animations).c_str());
    return outcome;
}

}  // namespace

/// Pop, hold the scoop, and the graph must walk the whole authored ladder.
/// The heelflip's authored clips are longer than the kickflip's, so it needs a
/// taller pop to reach the same count -- that is the authored gate, not a bug.
// #[ignore = "requires private stock graphs and animation assets"]
S3_TEST(held_flips_cycle_through_the_authored_double_triple_and_quad) {
    const auto* s = pb::stock();
    if (s == nullptr) return;
    for (const auto& [trick, boost] : {std::pair<std::string, float>{"Kickflip", 8.0f}, {"Heelflip", 10.0f}}) {
        const auto run = replay(*s, Input::held(trick, boost), 260);
        for (const char* count : {"", "2", "3", "4"}) {
            CHECK_MSG(contains(run.tricks, trick + count), trick << ": held ladder never published " << trick
                                                                 << count << "; saw "
                                                                 << pb::debug_strings(run.tricks) << " over "
                                                                 << pb::debug_strings(run.animations));
        }
        std::size_t cycles = 0;
        for (const auto& a : run.animations) {
            if (ends_with(a, "_CYC1") || ends_with(a, "_CYC2") || ends_with(a, "_CYC3")) ++cycles;
        }
        CHECK_MSG(cycles == 3u, trick << ": expected the three authored cycle clips, saw "
                                      << pb::debug_strings(run.animations));
    }
}

/// The ladder, landed: the quad has to reach the scorer as the quad's own
/// scorable and be named by the quad's authored label, not the single's.
// #[ignore = "requires private stock graphs and animation assets"]
S3_TEST(landed_quads_bank_and_name_the_quad_scorable) {
    const auto* s = pb::stock();
    if (s == nullptr) return;
    struct Case {
        std::string trick;
        float boost;
        std::string label;
    };
    for (const auto& c : {Case{"Kickflip", 8.0f, "#ID_TRICK_FLIP_QUADRUPLE_KICKFLIP"},
                          Case{"Heelflip", 10.0f, "#ID_TRICK_FLIP_QUADRUPLE_HEELFLIP"}}) {
        const auto run = replay(*s, Input::held(c.trick, c.boost), 300);
        CHECK_MSG(contains(run.tricks, c.trick + "4"), c.trick << ": the ladder no longer reaches the quad: "
                                                               << pb::debug_strings(run.tricks) << " over "
                                                               << pb::debug_strings(run.animations));
        CHECK_MSG(run.name == c.label, c.trick << ": the landed quad published " << run.name);
        // The ladder converts rather than accumulating, so only the quad's own
        // authored 250 is banked -- never the single's 100 as well.
        CHECK_MSG(run.reward > 250.0f, c.trick << ": the landed quad banked " << fmt::display_f32(run.reward));
    }
}

/// The single flip is the baseline the ladder must not fall below. The ladder
/// *converts* its carrier (`LINKS`), so each count replaces the previous one's
/// reward instead of adding to it -- a regression there would silently make a
/// quad worth less than a single.
// #[ignore = "requires private stock graphs and animation assets"]
S3_TEST(a_single_flip_is_the_ladder_baseline) {
    const auto* s = pb::stock();
    if (s == nullptr) return;
    const auto single = replay(*s, Input::single("Kickflip", 8.0f), 300);
    const auto quad = replay(*s, Input::held("Kickflip", 8.0f), 300);
    std::fprintf(stderr, "single=%s quad=%s\n", fmt::display_f32(single.reward).c_str(),
                 fmt::display_f32(quad.reward).c_str());
    CHECK_MSG(single.tricks == std::vector<std::string>{"Kickflip"}, "the released flick still cycled");
    CHECK_MSG(quad.reward >= single.reward, "the quad banked " << fmt::display_f32(quad.reward)
                                                               << " but the single banked "
                                                               << fmt::display_f32(single.reward));
}

/// An ollie, then the air scoop: `air.xml`'s `Lateflip` state, whose leaves name
/// `latekickflip`/`lateheelflip` and friends. These gestures live in their own
/// recognizer (`skater_air.pat`) and never enter the 270-row trick mapping, so
/// they reach the motion graph as bare intents.
// #[ignore = "requires private stock graphs and animation assets"]
S3_TEST(air_scoops_reach_the_authored_late_flips) {
    const auto* s = pb::stock();
    if (s == nullptr) return;
    struct Case {
        std::string gesture;
        std::string trick;
        std::string label;
    };
    for (const auto& c : {Case{"L_F_Kickflip", "LateKickflip", "#ID_TRICK_FLIP_LATE_KICKFLIP"},
                          Case{"L_F_Heelflip", "LateHeelflip", "#ID_TRICK_FLIP_LATE_HEELFLIP"}}) {
        const auto run = replay(*s, Input::late(c.gesture, c.trick), 260);
        CHECK_MSG(contains(run.tricks, c.trick), c.gesture << ": the air scoop never published " << c.trick
                                                           << "; saw " << pb::debug_strings(run.tricks) << " over "
                                                           << pb::debug_strings(run.animations));
        CHECK_MSG(run.name == c.label, c.gesture << ": the landed late flip published " << run.name);
    }
}
