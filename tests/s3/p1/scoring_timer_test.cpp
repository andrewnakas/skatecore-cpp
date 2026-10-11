// Port of skate-core/src/scoring/timer.rs (#[cfg(test)] mod tests).
#include "harness.hpp"
#include "s3/core/scoring/timer.hpp"

namespace {
using namespace s3::scoring::timer;

S3_TEST(native_ground_hold_preserves_previous_points_and_expiry_is_an_edge) {
    PointTimer timer{2.0f, false};
    CHECK(timer.advance(0.1f, 50.0f, 1.0f, true));
    CHECK_EQ(timer.points, 2.0f);
    CHECK(!timer.advance(0.1f, 50.0f, 1.0f, false));
    CHECK(timer.expired);
    timer.advance(0.1f, 50.0f, 1.0f, false);
    CHECK(!timer.expired);
}

/// What a bounded hold does when its bound runs out, which is what 82DA33E0's counter at
/// module+112 decides. The hold only arms while a single drain would land the timer under
/// 1.001 from a `previous` still above 1.000001, so that is a window of exactly one
/// drain's width: deny the hold once and, save for a sliver barely 0.001 wide, the timer
/// falls below the arming threshold and can never hold again. That is why a parked line
/// does expire, and why it can take more than one full window to do it.
S3_TEST(a_denied_hold_drops_the_timer_below_the_arming_threshold) {
    const float dt = 1.0f / 60.0f, drain = 50.0f, scale = 0.2f;
    const float step = drain * dt * scale;
    PointTimer timer{1.001f + step / 2.0f, false};
    // Armed: the points come straight back and the caller is told the hold took effect.
    const float before = timer.points;
    CHECK(timer.advance(dt, drain, scale, true));
    CHECK_EQ(timer.points, before);
    // Denied once, as the counter reaching its bound does.
    CHECK(!timer.advance(dt, drain, scale, false));
    CHECK(timer.points < 1.000001f);
    // From there the hold cannot arm again, whatever the caller asks for.
    CHECK(!timer.advance(dt, drain, scale, true));
    for (int i = 0; i < 8; ++i) {
        timer.advance(dt, drain, scale, true);
    }
    CHECK_EQ(timer.points, 0.0f);
}

S3_TEST(multiplier_uses_credited_points_and_survives_timer_decay) {
    const std::array<std::pair<float, float>, 3> levels = {{{50.0f, 1.5f}, {450.0f, 2.0f}, {800.0f, 3.0f}}};
    ComboTimer combo;
    combo.credit(50.0f, 801.0f, levels, 799.0f);
    CHECK_EQ(combo.multiplier, 1.5f);
    combo.credit(400.0f, 801.0f, levels, 799.0f);
    CHECK_EQ(combo.multiplier, 2.0f);
    combo.credit(350.0f, 801.0f, levels, 799.0f);
    CHECK_EQ(combo.multiplier, 3.0f);
    combo.timer.advance(8.0f, 100.0f, 1.0f, false);
    combo.credit(1.0f, 801.0f, levels, 799.0f);
    CHECK_EQ(combo.multiplier, 3.0f);
}
}  // namespace
