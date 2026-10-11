// Port of skate-core/src/scoring/session.rs.
// Module publication and line settlement, TU3 82DA37B0/82DA3B38.
#include "s3/core/scoring/session.hpp"

namespace s3::scoring::session {

float Session::publish_sequence(const Rules& rules, float landing_factor, bool penalized,
                                bool multiplier_enabled) {
    holder.reward_sequence(landing_factor);
    const Snapshot& snapshot = holder.snapshot;
    float raw = (snapshot.fingerflip_pending + snapshot.general_pending) + snapshot.accumulated;
    if (penalized && rules.bail_factor < 1.0f) {
        raw *= rules.bail_factor;
    }
    const float multiplier = multiplier_enabled ? combo.multiplier : 1.0f;
    if (multiplier_enabled) {
        combo.credit(raw, rules.combo_capacity, rules.combo_levels, rules.combo_refresh_threshold);
        if (combo.multiplier > rs::from_bits(0x3f8147aeu)) {
            line.credit(raw, rules.line_capacity);
        }
    }
    const float reward = multiplier * raw;
    holder.publish(reward, multiplier_enabled && line.points > 0.0f);
    return reward;
}

void Session::settle_line(bool reset_requested, bool collector_active) {
    if (reset_requested || line.expired) {
        line = PointTimer{};
        combo.multiplier = 1.0f;
        holder.finish_line();
    } else if (line.points <= 0.0f) {
        holder.bank_line(!collector_active);
    }
}

}  // namespace s3::scoring::session
