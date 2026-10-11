// Port of skate-core/src/scoring.rs.
// Native ScoreHolder accounting, TU3 82DA6198/6260/6408/6468/6538.
//
// Collectors own recognition and reward calculation. This ledger deliberately
// accepts their rewards rather than substituting a table of guessed points.
// Rendering and networking consume snapshots; neither advances accounting.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>

#include "s3/core/rs.hpp"

namespace s3::scoring {

inline constexpr std::size_t SCORABLE_COUNT = 332;
inline constexpr std::size_t SCORE_TYPE_COUNT = 14;

struct Scorable {
    std::size_t id = 0;
    /// Rust field `class` (a C++ keyword): renamed `class_`.
    /// Fixed metadata table **+12**, which retail treats as the score/display type.
    ///
    /// The two names here are the wrong way round against retail's own vocabulary, and
    /// renaming them now would touch every collector, so the offsets are what to trust:
    /// this is +12, read by CalcPointPenalty 82DA5D18 (`addi r7,r11,12`), the flip metric
    /// 82DA8EB8 and the class-3 bonus 82DA93D8.
    std::uint32_t class_ = 0;
    /// Fixed metadata table **+16**, which is retail's *class* -- an index into the
    /// `*_CLASS` list at 82084A40: `None, Air, Flip, FingerFlip, Grab, Grind, Handplant,
    /// NoComply, Manual, Slide, HippyJump, Revert, Boneless, Footplant`.
    ///
    /// Read by the Grind collector 82DA9D68 (`== 5`) and the Handplant collector 82DABA10
    /// (`== 6`), both via `addi r9,r27,16`.
    std::size_t score_type = 0;

    bool valid() const { return id < SCORABLE_COUNT && score_type < SCORE_TYPE_COUNT; }
    /// CalcPointPenalty82DA5D18 excludes metric class 5, and only class 5.
    ///
    /// The test is written as an `addic`/`subfe` carry trick rather than a compare:
    ///
    /// ```text
    /// lwzx  r11,r8,r7   ; class = table820862A8[id].class
    /// addi  r11,r11,-5  ; x = class - 5
    /// addic r6,r11,-1   ; r6 = x - 1, carry = (x != 0)
    /// subfe r11,r6,r11  ; r11 = ~r6 + x + carry = carry = (class != 5)
    /// ```
    ///
    /// which is 1 for every class but 5, and a 0 returns the unpenalised 1.0. Class 6,
    /// the 23 handplants, was excluded here too, so a handplant repeated within a line
    /// kept its full value instead of decaying down the authored curve.
    bool repetition_applies() const { return valid() && class_ != 5; }
};
S3_DERIVE_EQ(Scorable, id, class_, score_type)
inline std::ostream& operator<<(std::ostream& os, const Scorable& s) {
    return os << "Scorable { id: " << s.id << ", class: " << s.class_ << ", score_type: " << s.score_type
              << " }";
}

struct Snapshot {
    float completed_lines = 0.0f;     // 24
    float line = 0.0f;                // 28
    float accumulated = 0.0f;         // 32
    float last_reward = 0.0f;         // 36
    float general_pending = 0.0f;     // 40
    float fingerflip_pending = 0.0f;  // 44
    float grind_reward = 0.0f;        // 4776
};
S3_DERIVE_EQ(Snapshot, completed_lines, line, accumulated, last_reward, general_pending, fingerflip_pending,
             grind_reward)

class ScoreHolder {
public:
    Snapshot snapshot;

    bool has_pending_sequence() const { return pending_sequence_; }
    /// Non-air collectors use6198 then add directly to accumulator32.
    void credit_trick(Scorable scorable, float reward);
    void set_suppressed(bool suppressed) { suppressed_ = suppressed; }
    std::optional<std::int8_t> repetition_count(Scorable scorable) const;
    /// EndAirTrick82DA6260. Invalid IDs do not touch either history or rewards.
    void end_trick(Scorable scorable, float reward);
    /// Collector Exit publishes a pending sequence even for a cancelled exit.
    void finish_collector() { pending_sequence_ = true; }
    void cancel_pending();
    /// RewaredAirSequence82DA6468. Repeated calls cannot bank twice.
    void reward_sequence(float multiplier);
    /// PublishAndResetAirSequence82DA6538 receives the final reward from the
    /// module; it does not itself calculate the multiplier or bail penalty.
    void publish(float reward, bool add_to_line);
    void clear_sequence_history();
    /// Module82DA3B38 banks the line when its timer expires or reset is requested.
    void finish_line() { bank_line(true); }
    /// A zero line timer banks the current line even while a collector is
    /// active, but 82DA3B38 retains repetition in that case.
    void bank_line(bool clear_repetition);
    /// Native Reset82DA5C78 retains lifetime completed-lines (+24).
    void reset();

private:
    void count_trick(Scorable scorable);

    std::array<std::int8_t, SCORABLE_COUNT> repetitions_{};       // 4096
    std::array<std::int8_t, SCORABLE_COUNT> sequence_history_{};  // 4428
    std::array<std::int8_t, SCORE_TYPE_COUNT> type_history_{};    // 4760
    bool pending_sequence_ = false;                               // 7260
    bool suppressed_ = false;  // 7261 (physical output, not difficulty)
};

}  // namespace s3::scoring

// Rust `pub mod carrier; pub mod catalog; pub mod conversions; pub mod session; pub mod timer;`
#include "s3/core/scoring/carrier.hpp"
#include "s3/core/scoring/catalog.hpp"
#include "s3/core/scoring/conversions.hpp"
#include "s3/core/scoring/session.hpp"
#include "s3/core/scoring/timer.hpp"
