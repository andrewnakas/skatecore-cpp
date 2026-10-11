// Port of skate-core/src/scoring/carrier.rs.
// Scorable carrier lifecycle, TU3 82DA45C8/46D0/5DE0/5F98.
#include "s3/core/scoring/carrier.hpp"

namespace s3::scoring::carrier {

std::uint32_t delay_ticks(float authored_seconds, float extra_seconds) {
    // `(x as i64) as u32`: saturating float->i64, then truncating to u32.
    return std::uint32_t(rs::as_int<std::int64_t>((authored_seconds + extra_seconds) * rs::from_bits(0x42700000u)));
}

Carrier Carrier::new_(Scorable scorable, std::int32_t points, float factor, float announcement_threshold,
                      std::uint32_t start_tick, std::uint32_t delay_ticks, bool switch_, bool fakie) {
    Carrier c;
    c.scorable = scorable;
    c.points = points;
    c.factor = factor;
    c.reward = 0.0f;
    c.announcement_threshold = announcement_threshold * factor;
    c.start_tick = start_tick;
    c.delay_ticks = delay_ticks;
    c.announced = false;
    c.completed = false;
    c.unannounced = false;
    c.switch_ = switch_;
    c.fakie = fakie;
    return c;
}

void Carrier::credit(float unannounced_factor) {
    float r = float(points) * factor;
    if (unannounced) {
        r *= unannounced_factor;
    }
    reward += r;
}

bool Carrier::announce(std::uint32_t tick, float unannounced_factor) {
    if (announced || !scorable.valid() || std::uint32_t(tick - start_tick) < delay_ticks) {
        return false;
    }
    announced = true;
    credit(unannounced_factor);
    return true;
}

bool Carrier::complete(float unannounced_factor) {
    if (!scorable.valid()) {
        return false;
    }
    if (!announced) {
        unannounced = true;
        credit(unannounced_factor);
    }
    completed = true;
    return true;
}

void Carrier::convert_to(Carrier& replacement, float unannounced_factor) {
    completed = true;
    replacement.announced = true;
    replacement.credit(unannounced_factor);
}

}  // namespace s3::scoring::carrier
