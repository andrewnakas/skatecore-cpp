// Port of skate-core/src/animation/kickturn.rs.
//! Original Skate 3 TU3 KickTurnSteering: constructor82BAE730,
//! Begin82BAE430 and Update82BAE4A8. Curves are Globals324 anim_motion/kickturn.
#pragma once
#include "s3/core/point_graph.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::kickturn {

using point_graph::PointGraph;

struct Settings {
    PointGraph<8> spin;
    PointGraph<8> height;
};

/// Authored behavior fields28/32; constructor82BC7218 defaults both to0.5.
struct Parameters {
    float max_height = 0.0f;
    float spin_scale = 0.0f;
};
S3_DERIVE_EQ(Parameters, max_height, spin_scale)

struct Output {
    /// Published first, to the actual MotionGraph Balance attribute.
    float balance = 0.0f;
    float spin = 0.0f;
};
S3_DERIVE_EQ(Output, balance, spin)

//Source fsel(-value,0,value), then fsel(max-lower,lower,max).
//Preserves source unordered handling: a NaN selects the upper bound.
inline float bound(float value, float maximum) {
    const float lower = -value >= 0.0f ? 0.0f : value;
    return maximum - lower >= 0.0f ? lower : maximum;
}

struct State {
    float animation_length = 0.0f;
    float elapsed = 0.0f;
    bool first_update = true;
    float previous_height = 0.0f;
    float last_nonzero_intent = 0.0f;

    static State new_() { return State{0.0f, 0.0f, true, 0.0f, 0.0f}; }

    void begin(const Settings& settings) {
        *this = new_();
        //82BAE480 seeds the unscaled curve value, before applying max_height.
        previous_height = settings.height.evaluate(0.0f);
    }

    bool needs_animation_length() const { return first_update; }

    /// The caller first applies pending animation parameters, then reads the
    /// base animation tree's length. No time is added on that first update.
    void capture_animation_length(float length) { animation_length = length; }

    Output update(float dt, float intent, Parameters parameters, const Settings& settings) {
        if (!first_update) {
            elapsed += dt;
        }
        const float phase = bound(elapsed / animation_length, 1.0f);
        const float spin_curve = settings.spin.evaluate(phase);
        const float height_curve = settings.height.evaluate(phase);
        //82BAE660 fmadds, followed by separate82BAE670 fmuls. TU3 fixes
        //the blend coefficient at0.5; the S2 debug alternatives are absent.
        const float sum = rs::mul_add(height_curve, parameters.max_height, previous_height);
        const float height = bound(sum * 0.5f, parameters.max_height);
        previous_height = height;
        if (intent != 0.0f) {
            last_nonzero_intent = intent;
        }
        const float sign = last_nonzero_intent >= 0.0f ? 1.0f : -1.0f;
        const Output output{-height, (parameters.spin_scale * sign) * spin_curve};
        first_update = false;
        return output;
    }
};

}  // namespace s3::animation::kickturn
