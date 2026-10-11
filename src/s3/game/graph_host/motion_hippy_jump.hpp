// Port of skate-game/src/graph_host/motion_hippy_jump.rs (module `motion_hippy_jump`).
//! Owned implementation of `Sk8::Behaviours::HippyJumpAntic::Update`.
//!
//! Skate 3 TU3 IDA verifies the complete producer path: factory `0x82F8A340`
//! registers constructor `0x82BCBF60`; vtable `0x82321414` creates a private
//! elapsed-time record in `0x82BBCB50` and dispatches Update `0x82BBE900`.
//! Update advances that record by exactly 1/60 second, evaluates
//! `anim_motion/hippy_flip/antic_length_to_hippy_height`, and writes the
//! constructor-bound `TrickHeight` attribute. Skate 2 `0x82CA80F8` is only
//! corroborating evidence for the same observable algorithm.
#pragma once
#include <string>

#include "s3/core/point_graph.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/collections.hpp"

namespace s3::game::graph_host::motion_animation {
class MotionAnimation;
}

namespace s3::game::graph_host::motion_hippy_jump {

using motion_animation::MotionAnimation;

/// `pub(super)` in Rust.
struct Settings {
    s3::point_graph::PointGraph<8> antic_length_to_hippy_height;

    static rs::Result<Settings, std::string> load(const s3::data::Collections& data);
};

/// `pub(super)`; fields private in Rust (its tests read `elapsed`).
struct State {
    bool active = false;
    float elapsed = 0.0f;

    void begin() {
        active = true;
        elapsed = 0.0f;
    }
    void update(MotionAnimation& animation, const Settings& settings);
    void end() {
        active = false;
        elapsed = 0.0f;
    }
};

}  // namespace s3::game::graph_host::motion_hippy_jump
