// Port of skate-core/src/physics/skeleton_output/wobble.rs.
// SkateboardWobble constructor82BF2E48, trigger82BF2F18, update82BF2FB8.
#pragma once
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::physics::skeleton_output::wobble {

using point_graph::PointGraph;

struct Settings {
    PointGraph<8> takeoff_tilt{};
    PointGraph<8> landing_tilt{};
    PointGraph<8> takeoff_squish{};
    PointGraph<8> landing_squish{};
    float maximum_time = 0.0f;
};

struct Output {
    /// Native out0 reports whether this update evaluated the effect, including
    /// the final sample whose clock expires it. Return value is active AFTER.
    bool sampled = false;
    float tilt = 0.0f;
    float squish = 0.0f;
    bool remains_active = false;
};

class Wobble {
public:
    bool active = false;
    bool landing = false;
    float time = 0.0f;
    float amplitude = 0.0f;
    float direction = 1.0f;

    // Constructor copies landing curves while initializing type17=false.
    static Wobble default_() { return Wobble{}; }

    void trigger(bool landing_, bool reverse) {
        selected_landing_curves = landing_;
        landing = landing_;
        active = true;
        time = 0.0f;
        amplitude = 1.0f;
        direction = reverse ? -1.0f : 1.0f;
    }

    Output update(const Settings& settings) {
        if (!active) {
            return Output{};
        }
        const PointGraph<8>& tilt_curve = selected_landing_curves ? settings.landing_tilt : settings.takeoff_tilt;
        const PointGraph<8>& squish_curve =
            selected_landing_curves ? settings.landing_squish : settings.takeoff_squish;
        const float squish = squish_curve.evaluate(time) * amplitude;
        const float tilt = (tilt_curve.evaluate(time) * direction) * amplitude;
        time += rs::from_bits(0x3C888889u);
        if (time > settings.maximum_time) {
            active = false;
            time = 0.0f;
        }
        return Output{true, tilt, squish, active};
    }

private:
    bool selected_landing_curves = true;
};

/// PostWipeoutCheck82BD857C..86D4 modifies the observed board record only.
/// This wobble is not a force or a SetPartTransform on the simulated deck.
void apply(Output output, skeleton_animation_record::AnimationPartTransform& board);

}  // namespace s3::physics::skeleton_output::wobble
