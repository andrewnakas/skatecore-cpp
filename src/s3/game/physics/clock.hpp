// Port of skate-game/src/physics/clock.rs.
//! Offline tick cadence. Original TU3 Simulation::OnRequestSetSimRate82857EC8
//! sets a wall-clock timer; it does not change the physical integration step.
#pragma once
#include <cstdint>
#include <string>

#include "s3/core/camera/slow_motion.hpp"
#include "s3/game/common.hpp"
#include "s3/game/vfs_time.hpp"

namespace s3::game::physics::clock {

using vfs_time::Duration;

inline const float NORMAL_STEP = rs::from_bits(0x3c888889u);

/// Private in Rust.
Duration timer_period(std::int32_t frequency);

struct SimulationClock {
    std::uint32_t ticks_until_reset = 0;
    Duration timer_period_ = clock::timer_period(60);

    Status apply(sc::camera::SimulationRateRequest request);
    void finish_tick();
    Duration period() const { return timer_period_; }
};

}  // namespace s3::game::physics::clock
