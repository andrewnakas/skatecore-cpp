// Port of skate-core/src/scoring.rs.
// Native ScoreHolder accounting, TU3 82DA6198/6260/6408/6468/6538.
#include "s3/core/scoring.hpp"

namespace s3::scoring {

namespace {
std::int8_t saturating_add(std::int8_t a, std::int8_t b) {
    const int s = int(a) + int(b);
    if (s > 127) return 127;
    if (s < -128) return -128;
    return std::int8_t(s);
}
}  // namespace

void ScoreHolder::credit_trick(Scorable scorable, float reward) {
    if (!scorable.valid() || suppressed_) {
        return;
    }
    count_trick(scorable);
    snapshot.accumulated += reward;
}

void ScoreHolder::count_trick(Scorable scorable) {
    repetitions_[scorable.id] = saturating_add(repetitions_[scorable.id], 1);
    if (scorable.score_type != 0) {
        sequence_history_[scorable.id] = saturating_add(sequence_history_[scorable.id], 1);
        type_history_[scorable.score_type] = saturating_add(type_history_[scorable.score_type], 1);
    }
}

std::optional<std::int8_t> ScoreHolder::repetition_count(Scorable scorable) const {
    if (!scorable.valid()) return std::nullopt;
    return repetitions_[scorable.id];
}

void ScoreHolder::end_trick(Scorable scorable, float reward) {
    if (!scorable.valid() || suppressed_) {
        return;
    }
    count_trick(scorable);
    Snapshot& s = snapshot;
    if (scorable.class_ != 5) {
        s.general_pending += s.fingerflip_pending;
        s.fingerflip_pending = 0.0f;
    }
    if (scorable.class_ == 3) {
        s.fingerflip_pending += reward;
    } else {
        s.general_pending += reward;
    }
}

void ScoreHolder::cancel_pending() {
    if (pending_sequence_) {
        snapshot.general_pending = 0.0f;
        snapshot.fingerflip_pending = 0.0f;
    }
}

void ScoreHolder::reward_sequence(float multiplier) {
    if (!pending_sequence_) {
        return;
    }
    Snapshot& s = snapshot;
    s.accumulated += (s.general_pending + s.fingerflip_pending) * multiplier;
    s.general_pending = 0.0f;
    s.fingerflip_pending = 0.0f;
    pending_sequence_ = false;
}

void ScoreHolder::publish(float reward, bool add_to_line) {
    Snapshot& s = snapshot;
    if (add_to_line) {
        s.line += reward;
    }
    s.last_reward = reward;
    s.accumulated = 0.0f;
    s.general_pending = 0.0f;
    s.fingerflip_pending = 0.0f;
    clear_sequence_history();
}

void ScoreHolder::clear_sequence_history() {
    sequence_history_.fill(0);
    type_history_.fill(0);
    snapshot.grind_reward = 0.0f;
}

void ScoreHolder::bank_line(bool clear_repetition) {
    snapshot.completed_lines += snapshot.line;
    snapshot.line = 0.0f;
    if (clear_repetition) {
        repetitions_.fill(0);
    }
}

void ScoreHolder::reset() {
    const float completed_lines = snapshot.completed_lines;
    *this = ScoreHolder{};
    snapshot.completed_lines = completed_lines;
}

}  // namespace s3::scoring
