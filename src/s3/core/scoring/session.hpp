// Port of skate-core/src/scoring/session.rs.
// Module publication and line settlement, TU3 82DA37B0/82DA3B38.
#pragma once
#include <array>
#include <utility>

#include "s3/core/scoring.hpp"
#include "s3/core/scoring/timer.hpp"

namespace s3::scoring::session {

using timer::ComboTimer;
using timer::PointTimer;

struct Rules {
    float combo_capacity = 0.0f;
    std::array<std::pair<float, float>, 3> combo_levels{};
    float combo_refresh_threshold = 0.0f;
    float line_capacity = 0.0f;
    float bail_factor = 0.0f;
};

struct Session {
    ScoreHolder holder;
    ComboTimer combo;
    PointTimer line;

    /// The collector owns the decision to publish. Capture the multiplier
    /// before crediting timers: a threshold reached by this reward affects
    /// the next publication, not this publication's score (f29 at 82DA37B0).
    float publish_sequence(const Rules& rules, float landing_factor, bool penalized, bool multiplier_enabled);

    /// Reset/bail/output requests or the expiry edge reset line time and the
    /// multiplier. A merely empty line timer does not reset the multiplier.
    void settle_line(bool reset_requested, bool collector_active);
};

}  // namespace s3::scoring::session
