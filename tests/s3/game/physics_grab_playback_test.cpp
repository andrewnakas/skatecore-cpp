// Port of skate-game/src/tests/grab_playback.rs (mounted in physics.rs as `grab_tests`).
//! The air grab family: the grabs themselves, the fingerflips ("grab shove-its") thrown out of
//! them, and the one-foot and no-foot variants added on top.
//!
//! A grab is a held trigger plus a right-stick direction, and the authored `BoardAdjust`
//! branches turn the two into a named grab: `ActionGraphIncludes/BoardAdjustUp.xml` claims
//! `abs(BoardAdjustAngle) >= 2.355` and gives `LeftAirGrab` the tail and `RightAirGrab` the
//! seatbelt; `BoardAdjustDown.xml` claims `<= 0.785` and gives `RightAirGrab` the nose,
//! `LeftAirGrab` the crail and both triggers the rocket air. `BoardAdjustAngle` is
//! `atan2(x, -y)` (`trick_intentions::produce`), so raw right-stick `+y` is the up branch and
//! `-y` the down branch.
//!
//! From inside a grab, an authored `Fingerflip` scoop on the same stick reaches the
//! `.Grab.Grabbing.FingerFlip` child, whose `ScoringTrick` leaf names `tailgrab_fingerflip` /
//! `nosegrab_fingerflip`. The flick therefore has to be played *while the trigger is still
//! held*, which is what separates these from the ordinary air scoops in `flip_playback.rs`.
// The Rust tests are `#[ignore = "requires private stock graphs and animation assets"]`; here they
// run against the cleanroom pack and skip without it.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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

/// Xbox face buttons as `XboxState::buttons` bits (`input::xbox::convert` maps bits 12..15 to
/// slots 12..15). A is `RightPush`, X is `LeftPush`, B is `Dismount` -- the three inputs every
/// board-adjust grab's one-foot and no-foot children are gated on.
constexpr std::uint16_t A = 0x1000;
constexpr std::uint16_t B = 0x2000;
constexpr std::uint16_t X = 0x4000;

using Stick = std::array<std::int16_t, 2>;
constexpr Stick UP{0, 32767};
constexpr Stick DOWN{0, -32767};
constexpr Stick CENTRE{0, 0};

/// Takeoff boost for one grab plus one scoop. The flat course leaves about 0.77 s on its own
/// (see `docs/flip-ladder.md`), which the grab alone consumes.
constexpr float BOOST = 13.0f;
/// A two-scoop chain needs the settle, both scoops and the gap between them inside one air, and
/// running out mid-chain lands as a wipeout rather than a miss.
constexpr float CHAIN_BOOST = 20.0f;
/// Ticks of stick-only hold before the trigger, so the board-adjust branch is the selected leaf
/// when the grab arrives.
constexpr std::uint32_t STICK_LEAD = 6;
/// Ticks of held grab before the scoop, so `.Grab.Grabbing` is entered and the cycle is running.
constexpr std::uint32_t SETTLE = 22;
/// Ticks per authored coordinate, comfortably inside the recognizer's culling window.
constexpr std::uint32_t SCOOP_STEP = 3;
/// Ticks holding a finished scoop's last coordinate before the next one starts. This has to
/// outlast the *animation* the first scoop's transition began, not just the state change: the
/// grab-to-grab clips are `interruptable="false"`, and a gesture intent lives for a single tick,
/// so a scoop thrown while `F_FLIP_fs_GRAB_TO_Stale` is still playing is simply dropped.
constexpr std::uint32_t SCOOP_GAP = 30;

struct Outcome {
    std::vector<std::string> animations;
    std::vector<std::string> tricks;
    std::vector<std::string> grabs;
};

/// Every scorable the graph can publish from a grab, so an unexpected one is reported by name
/// rather than as a silent miss. Sourced from `scoring/catalog.rs`.
constexpr std::array<const char*, 42> WATCHED{
    "tailgrab",
    "tailgrab_left",
    "tailgrab_right",
    "tailgrab_fingerflip",
    "tailgrab_airwalk",
    "tailgrab_onefoot_front",
    "tailgrab_onefoot_behind",
    "nosegrab",
    "nosegrab_left",
    "nosegrab_right",
    "nosegrab_fingerflip",
    "nosegrab_airwalk",
    "nosegrab_onefoot_front",
    "nosegrab_onefoot_behind",
    "crailgrab",
    "crailgrab_left",
    "crailgrab_right",
    "crailtotail_fingerflip",
    "seatbeltgrab",
    "seatbeltgrab_left",
    "seatbeltgrab_right",
    "seatbelttonose_fingerflip",
    "rocketair",
    "nofoot_air",
    "christ_air",
    "fsgrab",
    "bsgrab",
    "fsgrab_fingerflip",
    "bsgrab_fingerflip",
    "dblgrab",
    "mutegrab",
    "mutegrab_left",
    "mutegrab_fingerflip",
    "stalegrab",
    "stalegrab_left",
    "stalegrab_fingerflip",
    "fsgrabtostalegrab_fingerflip",
    "bsgrabtomutegrab_fingerflip",
    "superman",
    "coffin",
    "fs_onefoot_right",
    "fs_backfoot",
};

float distance(std::array<float, 2> point, std::array<float, 2> to) {
    return std::hypot(point[0] - to[0], point[1] - to[1]);
}

std::vector<std::vector<std::array<float, 2>>> authored_all(const char* file, const pb::Stock& s,
                                                            const std::string& name) {
    std::vector<std::vector<std::array<float, 2>>> found;
    for (auto& p : pb::patterns(s, file)) {
        if (pb::eq_ignore_ascii_case(p.name, name)) found.push_back(std::move(p.points));
    }
    CHECK_MSG(!found.empty(), "authored " << name << " pattern in " << file);
    return found;
}

std::vector<std::array<float, 2>> authored(const char* file, const pb::Stock& s, const std::string& name) {
    auto all = authored_all(file, s, name);
    // Rust `swap_remove(0)`: the element at 0 is returned.
    return std::move(all[0]);
}

std::string hands_debug(const std::vector<std::size_t>& hands) {
    std::string out = "[";
    for (std::size_t i = 0; i < hands.size(); ++i) out += (i ? ", " : "") + std::to_string(hands[i]);
    return out + "]";
}
std::string stick_debug(Stick stick) { return "[" + std::to_string(stick[0]) + ", " + std::to_string(stick[1]) + "]"; }
std::string buttons_debug(std::uint16_t buttons) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%#06x", buttons);
    return buf;
}
std::string opt_f32(const float* v) { return v ? "Some(" + fmt::debug_f32(*v) + ")" : "None"; }

bool any_starts_with(const std::vector<std::string>& v, const std::string& prefix) {
    return std::any_of(v.begin(), v.end(), [&](const std::string& g) { return g.rfind(prefix, 0) == 0; });
}
bool any_eq(const std::vector<std::string>& v, const std::string& s) {
    return std::find(v.begin(), v.end(), s) != v.end();
}

Outcome replay_with(const pb::Stock& s, const std::vector<std::size_t>& hands, Stick stick,
                    const std::vector<std::string>& scoops, std::uint16_t buttons, float boost, std::uint32_t ticks) {
    auto physics = pb::unwrap(GamePhysics::load_with_terrain(*s.vfs, s.root, ground::Terrain::Course), "physics");
    auto skater = pb::unwrap(SkaterRuntime::load(*s.vfs, s.root, s.graphs, physics, "normal"), "skater");
    auto camera = pb::unwrap(camera::CameraRuntime::load(*s.vfs, s.root), "camera");
    input::controllers::ControllerInput controller{};
    auto controls = pb::unwrap(PlayerControls::load(*s.vfs, s.root), "controls");

    const auto ollie = authored("skater.pat", s, "Ollie");
    // GameInputManager 82696030 negates the mapped Y before matching, so the recognizer sees the
    // held grab direction with its Y flipped. Pick the authored variant that starts nearest that
    // point: the hold has already walked the pattern's first coordinate, so the flick only has to
    // complete it.
    std::array<float, 2> from{float(stick[0]) / 32767.0f, -float(stick[1]) / 32767.0f};
    // The air phase's stick track, one entry per tick: each scoop's coordinates in turn, then a
    // hold on its last so the state it reached has time to run before the next scoop starts.
    // Holding rather than centring matters -- centring drops the grab's own `BoardAdjustMag` and
    // exits the branch mid-trick.
    std::vector<std::array<float, 2>> track;
    for (const auto& name : scoops) {
        auto variants = authored_all("skater_fingerflip.pat", s, name);
        // Rust `sort_by` is stable.
        std::stable_sort(variants.begin(), variants.end(), [&](const auto& a, const auto& b) {
            return distance(a[0], from) < distance(b[0], from);
        });
        const auto chosen = variants[0];
        for (const auto& point : chosen) {
            for (std::uint32_t i = 0; i < SCOOP_STEP; ++i) track.push_back(point);
        }
        CHECK_MSG(!chosen.empty(), "authored scoop has coordinates");
        from = chosen.back();
        for (std::uint32_t i = 0; i < SCOOP_GAP; ++i) track.push_back(from);
    }
    std::vector<std::pair<std::string, s3::animation::output::attributes::AttributeName>> watched;
    for (const char* n : WATCHED) watched.emplace_back(n, encode(n));

    Outcome outcome{};
    std::uint32_t air_ticks = 0;
    for (std::uint32_t tick = 0; tick < ticks; ++tick) {
        if (tick == 60) {
            // A rolling approach, without a steering input that would also spin.
            pb::roll(physics, skater);
        }
        if (tick == 123) pb::boost(physics, skater, boost);
        const bool airborne = s3::player::state::category(skater.player_state.current()) == 200;
        Stick right{0, 0};
        std::array<std::uint8_t, 2> triggers{0, 0};
        if (!airborne) {
            // Crouch on the scoop's first coordinate, then flick it: the ordinary ground pop.
            if (tick >= 100 && tick < 112) {
                right = pb::sample(ollie[0]);
            } else if (tick >= 112 && tick < 118) {
                right = pb::sample(ollie[ollie.size() - 1]);
            }
        } else {
            // Stick before trigger. The air state selects a single leaf and `Grabbing` precedes
            // `BoardAdjusting` in document order, so a trigger held from the first airborne
            // frame lands in `Grabbing.LeftGrab` (a plain fsgrab) and the board-adjust branch is
            // never reached. Leading with the stick selects `BoardAdjusting.Up.Idle`, whose only
            // precondition is `BoardAdjustMag`, and the trigger then takes its authored
            // `<transition target="TailGrab"/>` from inside the branch.
            if (air_ticks >= STICK_LEAD) {
                for (const std::size_t hand : hands) triggers[hand] = 255;
            }
            if (air_ticks < SETTLE || track.empty()) {
                right = stick;
            } else {
                const std::size_t step = air_ticks - SETTLE;
                right = pb::sample(step < track.size() ? track[step] : track.back());
            }
        }
        // The push/dismount buttons only mean a one-foot or no-foot once the grab is holding;
        // pressed earlier they are an ordinary push and never reach the grab's own children.
        const std::uint16_t held_buttons = (airborne && air_ticks >= SETTLE) ? buttons : 0;
        const auto result = pb::tick(physics, skater, controls, controller, camera, s,
                                     pb::xbox(held_buttons, triggers, {0, 0}, right), true);
        if (!result.is_ok()) pb::panic("tick" + std::to_string(tick) + ": " + result.unwrap_err());
        if (s3::player::state::category(skater.player_state.current()) == 200) air_ticks += 1;

        const auto& motion = skater.animation.motion;
        // `TRACE_GRAB=1` prints the whole air, one line per tick. Every diagnosis in this file
        // came from reading it: which branch the AG chose, which gesture actually landed, and
        // which clip was still playing when it did.
        if (std::getenv("TRACE_GRAB") != nullptr && airborne) {
            auto ag = [&](const char* n) { return opt_f32(controls.action_intents.get(n)); };
            auto mg = [&](const char* n) { return opt_f32(motion.animation.motion_intents.get(n)); };
            std::fprintf(stderr,
                         "air%3u right=%s | AG angle=%s mag=%s L=%s R=%s ff=%s fsv=%s bsv=%s                  | MG "
                         "up=%s down=%s ff=%s ffs=%s | grab=%s anim=%s\n",
                         air_ticks, stick_debug(right).c_str(), ag("BoardAdjustAngle").c_str(),
                         ag("BoardAdjustMag").c_str(), ag("LeftAirGrab").c_str(), ag("RightAirGrab").c_str(),
                         ag("FingerFlip").c_str(), ag("FS_Varial").c_str(), ag("BS_Varial").c_str(),
                         mg("BoardAdjustUp").c_str(), mg("BoardAdjustDown").c_str(), mg("FingerFlip").c_str(),
                         mg("FingerFlipShuv").c_str(), outcome.grabs.empty() ? "None" : outcome.grabs.back().c_str(),
                         motion.animation.current_name.value_or("None").c_str());
        }
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
        if (motion.score_packet.grab) {
            const auto published = motion.score_packet.grab->first;
            const auto found = std::find_if(watched.begin(), watched.end(),
                                            [&](const auto& w) { return w.second == published; });
            if (found != watched.end() && (outcome.grabs.empty() || outcome.grabs.back() != found->first)) {
                outcome.grabs.push_back(found->first);
            }
        }
    }
    std::fprintf(stderr, "hands=%s stick=%s: grabs=%s tricks=%s\n  animations=%s\n", hands_debug(hands).c_str(),
                 stick_debug(stick).c_str(), pb::debug_strings(outcome.grabs).c_str(),
                 pb::debug_strings(outcome.tricks).c_str(), pb::debug_strings(outcome.animations).c_str());
    return outcome;
}

Outcome replay(const pb::Stock& s, const std::vector<std::size_t>& hands, Stick stick,
               const std::vector<std::string>& scoops, std::uint16_t buttons, std::uint32_t ticks) {
    return replay_with(s, hands, stick, scoops, buttons, BOOST, ticks);
}

}  // namespace

/// The grabs reachable from a single held trigger and a cardinal stick hold, with the trick the
/// authored graph names for each. Sites are from `docs/trick-reachability.md`.
// #[ignore = "requires private stock graphs and animation assets"]
S3_TEST(held_grabs_reach_their_authored_names) {
    const auto* s = pb::stock();
    if (s == nullptr) return;
    struct Case {
        std::vector<std::size_t> hands;
        Stick stick;
        std::string grab;
    };
    for (const auto& c : std::vector<Case>{
             {{0}, UP, "tailgrab"},
             {{1}, UP, "seatbeltgrab"},
             {{1}, DOWN, "nosegrab"},
             {{0}, DOWN, "crailgrab"},
             // Both triggers: the down branch's own child, not the stance-free double grab.
             {{0, 1}, DOWN, "rocketair"},
             // Centred stick keeps `BoardAdjustMag` at zero, so `Grabbing` wins the descent and the
             // stance-free double grab is what a two-trigger hold reaches.
             {{0, 1}, CENTRE, "dblgrab"},
         }) {
        const auto run = replay(*s, c.hands, c.stick, {}, 0, 300);
        CHECK_MSG(any_starts_with(run.grabs, c.grab),
                  c.grab << ": hands=" << hands_debug(c.hands) << " stick=" << stick_debug(c.stick)
                         << " never published it; saw grabs=" << pb::debug_strings(run.grabs)
                         << " tricks=" << pb::debug_strings(run.tricks) << " over "
                         << pb::debug_strings(run.animations));
    }
}

/// The tail and nose grab shove-its: hold the grab, then flick the authored `Fingerflip` scoop
/// without letting the trigger go.
// #[ignore = "requires private stock graphs and animation assets"]
S3_TEST(grab_fingerflips_reach_their_authored_tricks) {
    const auto* s = pb::stock();
    if (s == nullptr) return;
    struct Case {
        std::vector<std::size_t> hands;
        Stick stick;
        std::string trick;
    };
    for (const auto& c : std::vector<Case>{
             {{0}, UP, "tailgrab_fingerflip"},
             {{1}, DOWN, "nosegrab_fingerflip"},
         }) {
        const auto run = replay(*s, c.hands, c.stick, {"Fingerflip"}, 0, 300);
        CHECK_MSG(any_eq(run.tricks, c.trick),
                  c.trick << ": the held grab plus scoop never published it; saw tricks="
                          << pb::debug_strings(run.tricks) << " grabs=" << pb::debug_strings(run.grabs) << " over "
                          << pb::debug_strings(run.animations));
    }
}

/// The grab-to-grab shove-its. A varial scoop out of the seatbelt takes the authored
/// `<transition target="Down.NoseGrab"/>` and scores `seatbelttonose_fingerflip`; out of the
/// crail it takes `<transition target="Up.TailGrab"/>` and scores `crailtotail_fingerflip`.
/// These are the two that cross from one board-adjust branch into the other.
// #[ignore = "requires private stock graphs and animation assets"]
S3_TEST(grab_to_grab_fingerflip_shuvs_cross_the_board_adjust_branches) {
    const auto* s = pb::stock();
    if (s == nullptr) return;
    struct Case {
        std::vector<std::size_t> hands;
        Stick stick;
        std::string scoop;
        std::string trick;
    };
    for (const auto& c : std::vector<Case>{
             {{1}, UP, "BS_Varial", "seatbelttonose_fingerflip"},
             {{0}, DOWN, "BS_Varial", "crailtotail_fingerflip"},
         }) {
        const auto run = replay(*s, c.hands, c.stick, {c.scoop}, 0, 300);
        CHECK_MSG(any_eq(run.tricks, c.trick),
                  c.trick << ": the " << c.scoop << " scoop never published it; saw tricks="
                          << pb::debug_strings(run.tricks) << " grabs=" << pb::debug_strings(run.grabs) << " over "
                          << pb::debug_strings(run.animations));
    }
}

/// One foot, then the other, then neither. Every board-adjust grab carries the same three
/// children, gated on the push buttons rather than on the stick: `RightPush` (A), `LeftPush` (X),
/// and `Dismount` (B) or both pushes together for the no-foot airwalk.
// #[ignore = "requires private stock graphs and animation assets"]
S3_TEST(one_foot_and_no_foot_variants_reach_their_authored_grabs) {
    const auto* s = pb::stock();
    if (s == nullptr) return;
    struct Case {
        std::vector<std::size_t> hands;
        Stick stick;
        std::uint16_t buttons;
        std::string grab;
    };
    for (const auto& c : std::vector<Case>{
             {{0}, UP, A, "tailgrab_onefoot"},
             {{0}, UP, X, "tailgrab_onefoot"},
             {{0}, UP, B, "tailgrab_airwalk"},
             {{1}, DOWN, A, "nosegrab_onefoot"},
             {{1}, DOWN, X, "nosegrab_onefoot"},
             {{1}, DOWN, B, "nosegrab_airwalk"},
         }) {
        const auto run = replay(*s, c.hands, c.stick, {}, c.buttons, 300);
        CHECK_MSG(any_starts_with(run.grabs, c.grab),
                  c.grab << ": stick=" << stick_debug(c.stick) << " buttons=" << buttons_debug(c.buttons)
                         << " never published it; saw grabs=" << pb::debug_strings(run.grabs) << " over "
                         << pb::debug_strings(run.animations));
    }
}

/// The `Grabbing` branch's own extras. These hang off `GrabsTweaks` rather than off a
/// board-adjust direction, so the stick stays centred and no stick lead is needed. `Superman`
/// and `Coffin` are both authored `active="false"` and reached only by `DBLGrab`'s transitions:
/// superman wants the rising edge of `Dismount` on top of both triggers, coffin wants both
/// pushes instead.
// #[ignore = "requires private stock graphs and animation assets"]
S3_TEST(centred_grab_extras_reach_their_authored_names) {
    const auto* s = pb::stock();
    if (s == nullptr) return;
    struct Case {
        std::vector<std::size_t> hands;
        std::uint16_t buttons;
        std::string grab;
    };
    for (const auto& c : std::vector<Case>{
             {{0}, B, "nofoot_air"},
             {{1}, B, "christ_air"},
             {{0}, A, "fs_onefoot_right"},
             {{0}, X, "fs_backfoot"},
             {{0, 1}, B, "superman"},
             {{0, 1}, A | X, "coffin"},
         }) {
        const auto run = replay(*s, c.hands, CENTRE, {}, c.buttons, 300);
        CHECK_MSG(any_eq(run.grabs, c.grab),
                  c.grab << ": hands=" << hands_debug(c.hands) << " buttons=" << buttons_debug(c.buttons)
                         << " never published it; saw grabs=" << pb::debug_strings(run.grabs) << " over "
                         << pb::debug_strings(run.animations));
    }
}

/// The mute and stale grabs are not a stick direction -- `StaleMute` is authored
/// `active="false"` and reached only by `T_Grab.xml`'s `<transition target="Grabbing.StaleMute"/>`
/// out of a plain grab's `FingerFlipShuv`. So the recipe is two scoops: an `FS_Varial` out of the
/// fs/bs grab, which both scores the crossing trick and lands in stale/mute, then a `Fingerflip`
/// out of what it landed in.
// #[ignore = "requires private stock graphs and animation assets"]
S3_TEST(varial_out_of_a_plain_grab_reaches_the_stale_and_mute_family) {
    const auto* s = pb::stock();
    if (s == nullptr) return;
    struct Case {
        std::vector<std::size_t> hands;
        std::string crossing;
        std::string grab;
        std::string fingerflip;
    };
    for (const auto& c : std::vector<Case>{
             {{0}, "fsgrabtostalegrab_fingerflip", "stalegrab", "stalegrab_fingerflip"},
             {{1}, "bsgrabtomutegrab_fingerflip", "mutegrab", "mutegrab_fingerflip"},
         }) {
        const auto run = replay_with(*s, c.hands, CENTRE, {"FS_Varial", "Fingerflip"}, 0, CHAIN_BOOST, 360);
        for (const auto& expected : {c.crossing, c.fingerflip}) {
            CHECK_MSG(any_eq(run.tricks, expected),
                      expected << ": hands=" << hands_debug(c.hands)
                               << " never published it; saw tricks=" << pb::debug_strings(run.tricks)
                               << " grabs=" << pb::debug_strings(run.grabs) << " over "
                               << pb::debug_strings(run.animations));
        }
        CHECK_MSG(any_starts_with(run.grabs, c.grab), c.grab << ": the varial never landed in it; saw grabs="
                                                             << pb::debug_strings(run.grabs)
                                                             << " tricks=" << pb::debug_strings(run.tricks));
    }
}
