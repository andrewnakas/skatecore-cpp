// Port of skate-core/src/camera/slow_motion.rs.
//! SlowMotionController TU382BB1DD8/82BB1EE0/82BB20A8. The host owns
//! message delivery; this retains the curve, latching and timestep calculation.
#pragma once
#include <cstdint>
#include <utility>

#include "s3/core/point_graph.hpp"
#include "s3/core/rs.hpp"

namespace s3::camera {

struct SimulationRateRequest {
    float timestep = 0.0f;
    std::uint32_t ticks = 0;
};

struct SlowMotionSettings {
    point_graph::PointGraph<16> timescale;
    float fps_at_scale_one = 0.0f;

    SimulationRateRequest request(float progress) const {  // private in Rust
        const float curve = this->timescale.evaluate(progress);
        return SimulationRateRequest{1.0f / rs::mul_add(curve, this->fps_at_scale_one - 60.0f, 60.0f), 1};
    }
};

struct SlowMotionController {
    // private in Rust
    float elapsed = 0.0f;
    float air_duration = 0.0f;
    float time_before_prediction = 0.0f;
    bool latched_prediction = false;

    static std::pair<SlowMotionController, SimulationRateRequest> begin(const SlowMotionSettings& settings) {
        return {SlowMotionController{0.0f, 0.0f, 0.0f, false}, settings.request(0.0f)};
    }

    SimulationRateRequest update(float dt, float air_duration_, const SlowMotionSettings& settings) {
        this->elapsed += dt;
        //82BB1FC8 uses ble (not GT), which also takes the unordered case.
        float progress;
        if (!(air_duration_ > 0.0f)) {
            if (!this->latched_prediction) {
                this->time_before_prediction += dt;
            }
            progress = rs::from_bits(0x3dcccccd);
        } else {
            if (!this->latched_prediction) {
                this->air_duration = air_duration_;
                this->latched_prediction = true;
            }
            progress = this->elapsed / (this->time_before_prediction + this->air_duration);
        }
        return settings.request(progress);
    }

    static SimulationRateRequest end() { return SimulationRateRequest{rs::from_bits(0x3c888889), 0}; }
};

}  // namespace s3::camera
