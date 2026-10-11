// Port of skate-core/src/scoring/timer.rs.
// Point-backed countdowns, 82DA4C28 and module82DA37B0.
#include "s3/core/scoring/timer.hpp"

namespace s3::scoring::timer {

bool PointTimer::advance(float dt, float drain, float scale, bool hold_near_one) {
    expired = false;
    if (points > 0.0f) {
        const float previous = points;
        points = -((drain * dt) * scale - points);
        if (hold_near_one && points < 1.001f && previous > 1.000001f) {
            points = previous;
            return true;
        }
        if (points <= 0.0f) {
            points = 0.0f;
            expired = true;
        }
    }
    return false;
}

void ComboTimer::credit(float reward, float capacity, std::array<std::pair<float, float>, 3> levels,
                        float refresh) {
    timer.credit(reward, capacity);
    float next = 1.0f;
    for (const auto& [threshold, level_multiplier] : levels) {
        if (timer.points >= threshold) {
            next = level_multiplier;
        }
    }
    if (next > multiplier || reward > refresh) {
        multiplier = next;
    }
}

}  // namespace s3::scoring::timer
