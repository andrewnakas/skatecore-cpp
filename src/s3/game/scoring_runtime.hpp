// Port of skate-game/src/scoring_runtime.rs.
//! ScoreModule integration. The simulation owns recognition and accounting;
//! the APT movie only consumes the resulting publication.
// SIMREF: HUD coupling (apt_vm::Value, hud_runtime::Input, Runtime::hud_input) removed.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "s3/core/animation/landing_quality.hpp"
#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/physics/filtered_state.hpp"
#include "s3/core/scoring/carrier.hpp"
#include "s3/core/scoring/catalog.hpp"
#include "s3/core/scoring/conversions.hpp"
#include "s3/core/scoring/session.hpp"
#include "s3/data/collections.hpp"
#include "s3/data/scoring.hpp"
#include "s3/game/common.hpp"

namespace s3::game::scoring_runtime {

using ::s3::animation::output::attributes::AttributeName;
using ::s3::data::ScoringData;
using ::s3::physics::filtered_state::FilteredCategory;
using ::s3::scoring::carrier::Carrier;
using ::s3::scoring::session::Session;

/// `air_horizontal_distance`. The five air metrics are consecutive from here.
inline constexpr std::size_t AIR_METRIC_BASE = 129;

/// The localisation ids `sub_825E51A0` carries as string literals at 8220E238..8220E494.
///
/// They live in the executable, not in the vault -- searching the owner's collections for
/// `ID_TRICK_AIR*` finds nothing -- so a port has to hold them as literals too. The HUD's
/// own language table does resolve them: `Air`, `FS`, `BS`, `Frontflip`, `Backflip`.
namespace trick_tokens {
/// 8220E3AC. The bare fallback when no record has been announced.
inline constexpr const char* AIR = "ID_TRICK_AIR";
/// 8220E254 / its BS sibling.
inline constexpr const char* FS_SPIN = "ID_TRICK_AIR_FS_SPIN";
inline constexpr const char* BS_SPIN = "ID_TRICK_AIR_BS_SPIN";
/// 8220E474 and 8220E454, appended by the sign of the flip direction.
inline constexpr const char* FRONTFLIP = "ID_TRICK_AIR_METRICS_FRONTFLIP";
inline constexpr const char* BACKFLIP = "ID_TRICK_AIR_METRICS_BACKFLIP";
}  // namespace trick_tokens

// ---- private free items (declared for the ported tests) ----

/// Whether a scorable's name may carry a spin, `r23` in `825E51A0`. (See the Rust doc for the
/// admitted set and the unported `desc+12 != 60` grab qualifier.)
bool decorates_spin(std::uint32_t trick_type);

/// Compose the displayed trick name the way `sub_825E51A0` does. (`base: Option<&str>` -> nullable.)
std::string compose_trick_name(const char* base, std::int32_t spin_turns, std::int32_t flip_direction, bool switch_,
                               bool goofy, bool decorate_spin);

/// One of the four run accumulators the gap/context collector keeps, `82DA7B50`.
///
/// Each measures how far the skater travelled horizontally while its own condition held,
/// across as many separate runs as the air contains. Retail also keeps the longest single
/// run at +24 and a run count at +28; neither reaches the score.
struct ContextRun {
    /// +0, the position the current run started from.
    std::array<float, 3> start{};
    /// +16, the distance covered by the run in progress.
    float current = 0.0f;
    /// +20, the distance of every run already closed.
    float banked = 0.0f;
    /// byte +32.
    bool running = false;

    void advance(std::array<float, 3> position, bool active);
    /// `82DA88E8` sums the closed runs and the one in progress before the curve.
    float total() const { return banked + current; }
};

/// The curves `82DA88E8` evaluates, in the order `82DA89A0` drives the runs.
///
/// `addi r10,r11,1440` / `1280` / `1360` / `1200` against the collector tuning block.
inline constexpr std::array<std::uint16_t, 4> CONTEXT_CURVES = {0x5a0, 0x500, 0x550, 0x4b0};

/// `air_metric`, the scorable `82DA8550` banks the gap/context total under.
inline constexpr std::size_t CONTEXT_METRIC = 237;

float dot3(std::array<float, 3> a, std::array<float, 3> b);
std::array<float, 3> cross3(std::array<float, 3> a, std::array<float, 3> b);
std::optional<std::array<float, 3>> normalize3(std::array<float, 3> v);

/// One frame of `82DAC9D0` with axis selector 0, as `82DAC880` consumes it.
///
/// This is the yaw of a transform's own **forward** (row 2) about its own **up** (row 1),
/// measured against the accumulator's stored reference -- *not* a world-Y heading.
float spin_angle(std::array<float, 3> reference_up, std::array<float, 3> reference_forward,
                 std::array<float, 3> forward);

/// `Handplant_CLASS`, index 6 of the `*_CLASS` list at 82084A40.
inline constexpr std::size_t HANDPLANT_CLASS = 6;

/// `SKATE_SCORING_TRACE=1` prints what every air and every publication actually scored.
bool trace_enabled();

enum class Collector {
    None,
    Ground,
    Air,
    Grind,
    Offboard,
    Handplant,
};

struct Frame {
    std::uint32_t tick = 0;
    float dt = 0.0f;
    FilteredCategory category{};
    std::uint32_t state = 0;
    std::optional<AttributeName> descriptor;
    std::int32_t grind_id = 0;
    std::uint32_t flags = 0;
    std::array<float, 3> position{};
    std::array<float, 3> velocity{};
    std::array<float, 3> forward{};
    /// PhysOut_Skeleton +368 rows 1 and 2 -- the rider skeleton root's own up and forward.
    ///
    /// This, and not the deck, is what retail's spin accumulator measures. 82DA8BE0 feeds
    /// accumulator A `[[owner+20]+368]` on every one of its four branches, and A's angle is
    /// the only one that ever reaches the banked total; the board-derived accumulator B is
    /// held at zero for as long as a named trick is carried.
    std::array<float, 3> rider_up{};
    std::array<float, 3> rider_forward{};
    bool switch_ = false;  // `switch`
    bool fakie = false;
    bool nollie = false;
    bool body_flip = false;
    /// Air 445. Selects the flip's direction; see [`Runtime::flip_direction`].
    bool body_flip_side = false;
    /// PhysOut.Ground +192: the hips line test's ray origin, the hips part's world
    /// position. `82DB6EC0` publishes it unconditionally.
    std::array<float, 3> hips_position{};
    /// PhysOut.Ground +208, and `None` when Ground +320 says the test missed. The hips
    /// test's hit position, republished only while `player+1524` is set.
    std::optional<std::array<float, 3>> hips_ground;
    /// PhysOut.Ground +260: the packed surface tag the hips test hit.
    std::uint32_t hips_surface = 0;
    bool suspend_air = false;
    ::s3::animation::landing_quality::Output landing{};
    bool teleported = false;
    bool reverting = false;
    /// PhysOut -> State byte 70, the ground collector's manual gate at 82DAA8E0.
    /// 82DB6EC0 fills it from `[base+1888]+56`; this host publishes no such byte, so it
    /// is always false. It is **not** State+66, the revert's active lifetime.
    bool manual_block_70 = false;
};

struct Runtime {
    ScoringData data;
    Session session;
    // Private in Rust.
    Collector collector = Collector::None;
    std::array<std::optional<Carrier>, 4> carriers{};
    std::array<float, 4> held{};
    std::array<float, 4> distance{};
    std::array<float, 4> metric_rewards{};
    std::array<bool, 4> metric_started{};
    std::array<float, 3> start{};
    std::array<float, 3> previous{};
    /// The accumulator's stored reference rows, `82DAC880`'s `obj[0..48]`.
    std::pair<std::array<float, 3>, std::array<float, 3>> spin_reference{{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
    float spin = 0.0f;
    float peak = 0.0f;
    float air_factor = 1.0f;
    float air_repetition = 1.0f;
    bool air_repetition_set = false;
    std::uint32_t grab_chain = 0;
    std::array<float, 5> air_metrics{};
    /// 82DA8BE0's +2344: the air's rotation quantised to whole half-turns and signed by
    /// its direction. The reward uses only its magnitude; the sign is for the trick name.
    std::int32_t spin_turns = 0;
    /// 82DA8EB8's +2348: `0` until a body flip is credited, then `+1` or `-1` for its
    /// side. Retail writes it only while an announced grab carrier is current, so an
    /// ungrabbed flip earns no flip reward -- but it is still *named* as a flip.
    std::int32_t flip_direction = 0;
    /// The gap/context collector's four run accumulators, at collector +192/+240/+288/+336.
    std::array<ContextRun, 4> context_runs{};
    /// The hips ray origin's height at take-off. (`82DA7DA0` air collector Reset snapshot.)
    float takeoff_hips_height = 0.0f;
    /// 82DA93D8's one-shot latch at +777: the class-3 bonus has already been paid for
    /// this air, and cannot be paid again however many flip tricks follow.
    bool flip_bonus_paid = false;
    /// 82DA8EB8's +2396 latch, which is also scoring output byte 14651: a body flip
    /// happened during this air, whatever the carrier was.
    bool flip_seen = false;
    std::uint32_t landing_countdown = 0;
    std::uint32_t idle_ticks = 0;
    /// ScoreModule+112. 82DA33E0 counts the consecutive frames the line timer's near-one
    /// hold actually took effect while the *ground* collector is the active one, and caps
    /// the hold at `[collector 0x5fc] * 60` frames. Any frame that does not hold resets it.
    std::uint32_t ground_hold_ticks = 0;
    std::uint32_t collector_ticks = 0;
    /// The ground collector's own counter at +932: frames the manual slot has been
    /// suppressed by State+70, which readmits it only while the count is <= 6.
    std::uint32_t manual_block_ticks = 0;
    std::optional<std::size_t> revert_id;
    bool sequence_active = false;
    float sequence_score = 0.0f;
    /// The composed name the display shows, rebuilt each tick by [`compose_trick_name`].
    std::string trick_name;
    /// The announced record's own localisation id, before spin and flip are added.
    std::optional<std::string> base_trick_label;
    /// The named scorable's authored TrickType, which decides whether the name may carry a
    /// spin at all. See [`decorates_spin`].
    std::uint32_t base_trick_type = 0;
    std::array<bool, 4> stance{};
    bool clean = false;
    bool sketchy = false;
    bool new_trick = false;
    bool modified_trick = false;
    bool close_tricks = false;
    /// The spin and flip the *display* holds, `B+52`/`B+56` by way of `M+12`/`M+16`.
    /// (Only the active Air collector's publisher 82DA9A18 refreshes them; 827743A0 clears them.)
    std::int32_t published_spin_turns = 0;
    std::int32_t published_flip_direction = 0;
    /// The multiplier this runtime last reported, `52(r31)` in 82666BC0.
    float published_multiplier = 1.0f;
    /// Set for the one tick on which the published sequence multiplier changed, to the value
    /// it changed to. 82666BC0 polls `140(r30)` against its own cached copy and reacts to any
    /// change in either direction; the ScoreModule itself plays nothing.
    std::optional<float> multiplier_changed;

    static rs::Result<Runtime, std::string> load(const sd::Collections& data);
    /// Uses the same landing-quality thresholds as the retail scoring/HUD path without requiring
    /// audio to infer clean or sketchy from animation names.
    std::pair<bool, bool> classify_landing_audio(::s3::animation::landing_quality::Output landing) const;
    /// Whether a sequence is open, i.e. whether the collectors are still building one.
    bool sequence_open() const { return sequence_active; }
    /// The composed name the display is showing, as [`compose_trick_name`] last built it.
    const std::string& displayed_name() const { return trick_name; }
    Status advance(const Frame& f);

    // Private in Rust.
    float penalty(std::size_t id) const;
    void finish(std::size_t slot, bool complete, bool keep_metric);
    Status carrier(std::size_t slot, std::optional<std::size_t> id, const Frame& f);
    void advance_context_runs(const Frame& f);
    /// `82DA88E8`: the four runs through their curves. Banked as scorable 237 at the
    /// landing, and shown live in the sequence total meanwhile.
    float context_reward() const;
    /// 82DA93D8's one-shot class-3 bonus: `reward += 0x600 * points * factor`.
    void pay_flip_bonus(const Frame& f);
    /// The active collector's answer to "does the sequence continue?", vtable slot 5.
    bool sequence_continues(const Frame& f) const;
};

}  // namespace s3::game::scoring_runtime
