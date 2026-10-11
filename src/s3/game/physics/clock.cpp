// Port of skate-game/src/physics/clock.rs.
#include "s3/game/physics/clock.hpp"

#include <climits>

namespace s3::game::physics::clock {

Status SimulationClock::apply(sc::camera::SimulationRateRequest request) {
    //82857F44..7C rounds the reciprocal to an integer timer frequency.
    const float frequency_f = rs::trunc(1.0f / request.timestep + 0.5f);
    if (!rs::is_finite(frequency_f) || frequency_f < 1.0f || frequency_f > float(INT32_MAX)) {
        return rs::make_err(fmt::cat("Invalid simulation-rate request: ", request.timestep));
    }
    const std::int32_t frequency = rs::as_i32(frequency_f);
    if (10000000 / frequency == 0) {
        return rs::make_err(std::string("Simulation-rate request has a zero timer period"));
    }
    timer_period_ = timer_period(frequency);
    if (request.timestep == NORMAL_STEP) {
        ticks_until_reset = 0;
    } else if (static_cast<std::int32_t>(request.ticks) > 0) {
        ticks_until_reset = request.ticks + 1u;
    } else {
        //TU3 hardcodes180 at82857F34; S2 used a debug setting here.
        ticks_until_reset = 180;
    }
    return ok();
}

void SimulationClock::finish_tick() {
    //SimUpdate2 8285A694..A6C8 runs after the camera publication. Pending
    //camera messages are dispatched at the start of the following tick.
    ticks_until_reset = ticks_until_reset - 1u;
    if (ticks_until_reset == 0) {
        timer_period_ = timer_period(60);
    }
}

Duration timer_period(std::int32_t frequency) {
    //Original timer setter82966910..2C: integer division in100ns units.
    return Duration::from_nanos(static_cast<std::uint64_t>(10000000 / frequency) * 100u);
}

}  // namespace s3::game::physics::clock
