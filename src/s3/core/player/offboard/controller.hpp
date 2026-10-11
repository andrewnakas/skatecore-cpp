// Port of skate-core/src/player/offboard/controller.rs (+ output.rs, placement.rs,
// state.rs, step.rs).
// Canonical Biped owner and original ground dispatcher82D7C818.
// Constructor82D7AFD8/reset82D7B1C0; S3 SHA256
// 431b8eba23565affdc10d137df19b06fe286244cefb3e1a13f32693e9600395a.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/player/offboard/controller/output.hpp"
#include "s3/core/player/offboard/controller/placement.hpp"
#include "s3/core/player/offboard/controller/state.hpp"
#include "s3/core/player/offboard/movement_intent.hpp"
#include "s3/core/player/offboard/movement_velocity.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::player::offboard::controller {

using point_graph::PointGraph;

/// Validated stock physics_biped curves, supplied by the data boundary.
struct Settings {
    movement_intent::Settings movement_intent;
    movement_velocity::Settings movement_velocity;
    PointGraph<8> slide_vs_slope{};
    PointGraph<8> slide_vs_speed{};
};

/// Actual448-byte ground-job content as typed values.
/// No default implementation in Rust: a caller must supply its actual observations.
struct GroundJob {
    Vector contact_position{};    // 0
    Vector contact_normal{};      // 16
    Frame support_frame{};        // 32..80
    Vector target_position{};     // 96
    Vector target_normal{};       // 112
    Vector edge_position{};       // 128
    Vector edge_normal{};         // 144
    std::uint32_t flags = 0;      // 176
    std::uint32_t support_id = 0;  // 180
    std::array<Vector, 2> collision_displacements{};  // 192/208
    Vector animation_motion{};    // 224
    Vector animation_velocity{};  // 240
    Vector desired_direction{};   // 256
    Vector animation_position{};  // 272
    float requested_duration = 0.0f;  // 288
    bool mirrored = false;            // 292
    float requested_phase = 0.0f;     // 296
    float override_duration = 0.0f;   // 300
    bool animation_directed = false;  // 304
    float movement = 0.0f;            // 308
    float steering = 0.0f;            // 312
    bool sprint_pressed = false;      // 316
    bool suppress_lean = false;       // 317
    bool suppress_minimum = false;    // 318
    bool target_frame_present = false;  // 352
    bool edge_active = false;           // 353
    Frame target_frame{};               // 368..416
    bool ignore_obstacle = false;       // 432
};

/// output.rs exporter82D80F48 (pub(super)).
GroundResult export_(const State& s);

/// Run the recovered ground producer against an externally owned state.
GroundResult step_ground(State& state, const Settings& settings, const GroundJob& job);

namespace step {
/// 82D7C818 ordering. Every shared writeback precedes its next reader (pub(super)).
void update(State& s, const Settings& c, const GroundJob& j);
}  // namespace step

struct Controller {
    State state;
    Settings settings;

    /// Metrics are actual OffBoard walk/run/sprint AnimTransZ query results.
    /// None means the original query returned absent, whose native speed is0.
    static Controller new_(Settings settings, const std::array<std::optional<ClipMetric>, 3>& metrics) {
        return Controller{State::new_(metrics), std::move(settings)};
    }
    /// Original Reset preserves cadence and positional-correction vectors.
    void reset() { state.reset(); }
    void place(const PlacementInput& input) { state.place(input); }
    GroundResult step_ground(const GroundJob& job) { return controller::step_ground(state, settings, job); }
};

}  // namespace s3::player::offboard::controller
