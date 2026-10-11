// Port of skate-game/src/graph_host/motion_finger_flip.rs (module `motion_finger_flip`).
//! FingerFlipOut: TU3 Begin82BB3198, Update82BB7C40, empty End82B61BB8.
#pragma once
#include <string>

#include "s3/core/point_graph.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/collections.hpp"

namespace s3::game::graph_host::motion_finger_flip {

/// `pub(super)`; fields private in Rust (constructed directly by its tests).
struct Settings {
    float minimum = 0.0f;
    float maximum = 0.0f;
    s3::point_graph::PointGraph<8> curve;

    static rs::Result<Settings, std::string> load(const s3::data::Collections& data);
};

struct State {
    float elapsed = 0.0f;

    void begin() { elapsed = 0.0f; }
    float update(bool grab_present, float dt, const Settings& settings) {
        //82454FD8 tests membership, including a zero-valued intent. The time
        //service supplies dt; this is not the magnitude of the grab input.
        elapsed = grab_present ? elapsed - dt : elapsed + dt;
        elapsed = rs::clamp(elapsed, settings.minimum, settings.maximum);
        return settings.curve.evaluate(elapsed);
    }
};

}  // namespace s3::game::graph_host::motion_finger_flip
