// Port of skate-game/src/tests/revert_playback.rs (mounted in physics.rs as `revert_tests`).
//! Reverts, which are neither a flip nor a grab and are not named by any scoring leaf.
//!
//! `MotionGraphIncludes/revert.xml`'s `Fs`/`Bs` children are gated on `SlideFs180` / `SlideBs180`
//! -- left-stick patterns from `skaterls.pat`, not right-stick trick scoops -- and each carries
//! `<behaviour name="SetScoreAugmentation" augment="FSRevert" mirrorAugment="BSRevert"/>`.
//!
//! That augmentation is the whole crediting path. `ScorePacket::set` turns the augmentation
//! index into a flag bit (`FSRevert` is bit 29, `BSRevert` bit 28) and `scoring_runtime` reads
//! those two bits straight into `revert_id` 4 and 5. No `ScoringTrick` leaf ever spells
//! `fsrevert`, which is why a name-only reachability audit reports both as unreachable.
// The Rust test is `#[ignore = "requires private stock graphs and animation assets"]`; here it runs
// against the cleanroom pack and skips without it.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "physics_playback_support.hpp"

using namespace s3::game;
using namespace s3::game::physics;
namespace pb = s3test::game::playback;

namespace {

/// Tick the left stick first deflects on, well after the roll-in has settled.
constexpr std::uint32_t ENTRY = 120;
/// Ticks holding the pattern's entry coordinate before the sweep; see the note in `replay`.
constexpr std::uint32_t HOLD = 30;

struct Outcome {
    std::vector<std::string> animations;
    std::uint32_t augmentation = 0;
};

Outcome replay(const pb::Stock& s, const std::string& gesture, std::uint32_t ticks) {
    auto physics = pb::unwrap(GamePhysics::load_with_terrain(*s.vfs, s.root, ground::Terrain::Course), "physics");
    auto skater = pb::unwrap(SkaterRuntime::load(*s.vfs, s.root, s.graphs, physics, "normal"), "skater");
    auto camera = pb::unwrap(camera::CameraRuntime::load(*s.vfs, s.root), "camera");
    input::controllers::ControllerInput controller{};
    auto controls = pb::unwrap(PlayerControls::load(*s.vfs, s.root), "controls");

    // The slides live on the left stick, in their own recognizer.
    std::vector<std::array<float, 2>> scoop;
    bool found = false;
    for (auto& p : pb::patterns(s, "skaterls.pat")) {
        if (pb::eq_ignore_ascii_case(p.name, gesture)) {
            scoop = std::move(p.points);
            found = true;
            break;
        }
    }
    if (!found) pb::panic("authored " + gesture + " pattern in skaterls.pat");

    Outcome outcome{};
    for (std::uint32_t tick = 0; tick < ticks; ++tick) {
        if (tick == 60) {
            // A rolling approach, without a steering input that would also spin.
            pb::roll(physics, skater);
        }
        // Hold the pattern's entry coordinate, then sweep the rest one coordinate per tick.
        //
        // Both halves matter. Sweeping from a centred stick never produces a `SlideFs180` at
        // all: `BackFlip`'s first authored coordinate sits within tolerance of centre, so idling
        // has already advanced its node, and the arc's second point completes it one tick before
        // the slide's third. Only one pattern wins a file, `permitted` then drops `BackFlip`
        // because no grab is held, and the file yields nothing. Entering from the deflected
        // stick leaves `BackFlip` parked at its first coordinate -- which is also how the trick
        // is really thrown, out of a turn or a powerslide, per `ground.xml`'s transitions into
        // `Revert`.
        //
        // The sweep itself has to be one coordinate per tick: the recognizer culls a node after
        // `NumTicksPatternNotInRangeBeforeCulling` ticks not yet within tolerance of the next
        // point, and that budget is read per stick, the left stick's being the tighter.
        std::array<std::int16_t, 2> left{0, 0};
        if (tick >= ENTRY) {
            const std::uint32_t since = tick - ENTRY;
            const std::size_t step = since > HOLD ? since - HOLD : 0;  // saturating_sub
            if (step < scoop.size()) left = pb::sample(scoop[step]);
        }
        const auto result =
            pb::tick(physics, skater, controls, controller, camera, s, pb::xbox(0, {0, 0}, left, {0, 0}), true);
        if (!result.is_ok()) pb::panic(gesture + " tick" + std::to_string(tick) + ": " + result.unwrap_err());

        if (std::getenv("TRACE_REVERT") != nullptr && tick >= 148 && tick < 175) {
            const auto& snapshot = skater.scoring.session.holder.snapshot;
            std::fprintf(stderr, "tick%u left=[%d, %d] packet=%s line=%.2f acc=%.2f last=%.2f anim=%s\n", tick,
                         left[0], left[1], pb::hex8(skater.animation.motion.score_packet.flags).c_str(),
                         double(snapshot.line), double(snapshot.accumulated), double(snapshot.last_reward),
                         skater.animation.motion.animation.current_name.value_or("None").c_str());
        }
        outcome.augmentation |= skater.animation.motion.score_packet.flags;
        const auto& name = skater.animation.motion.animation.current_name;
        if (name && (outcome.animations.empty() || outcome.animations.back() != *name)) {
            outcome.animations.push_back(*name);
        }
    }
    std::fprintf(stderr, "%s: flags=%s\n  animations=%s\n", gesture.c_str(), pb::hex8(outcome.augmentation).c_str(),
                 pb::debug_strings(outcome.animations).c_str());
    return outcome;
}

}  // namespace

/// Both directions reach their authored `Revert` child and publish its score augmentation.
///
/// The assertion is the augmentation bit and the authored clip, not a ledger credit. A revert
/// extends a line rather than opening one, so thrown on its own -- with no trick before it --
/// nothing is banked and the snapshot stays at zero. That is retail behaviour, not a gap: the
/// ledger side of the path is already exercised live, `fsrevert` having been credited in a
/// recorded session (see `docs/trick-reachability.md`).
// #[ignore = "requires private stock graphs and animation assets"]
S3_TEST(left_stick_slides_reach_both_reverts) {
    const auto* s = pb::stock();
    if (s == nullptr) return;
    struct Case {
        std::string gesture;
        std::uint32_t bit;
        std::string clip;
    };
    for (const auto& c : {Case{"SlideFs180", 0x20000000u, "B_FS_REVERT"},
                          Case{"SlideBs180", 0x10000000u, "B_BS_REVERT"}}) {
        const auto run = replay(*s, c.gesture, 320);
        CHECK_MSG((run.augmentation & c.bit) != 0, c.gesture << ": the graph never published the augmentation; flags="
                                                             << pb::hex8(run.augmentation) << " over "
                                                             << pb::debug_strings(run.animations));
        CHECK_MSG(std::find(run.animations.begin(), run.animations.end(), c.clip) != run.animations.end(),
                  c.gesture << ": the augmentation fired without the authored clip " << c.clip << "; saw "
                            << pb::debug_strings(run.animations));
    }
}
