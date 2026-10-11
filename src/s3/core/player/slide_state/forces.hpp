// Port of skate-core/src/player/slide_state/forces.rs (whole file).
// Full non-launch arithmetic82D3AB14..AE4C and point force82D925E0.
// Uses independent PC arithmetic with the original refinement/operation order.
#pragma once
#include <array>

#include "s3/core/physics/force_queue.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::player::slide_state::forces {

using V = std::array<float, 4>;

struct SlideSettings {
    point_graph::PointGraph<8> input_remap{};
    point_graph::PointGraph<8> remap_vs_speed{};
    point_graph::PointGraph<8> force_vs_angle{};
    point_graph::PointGraph<8> force_vs_speed{};
    float softest_wheel_force = 0.0f;
    float softest_wheel_spin = 0.0f;
    float angular_force = 0.0f;
    float force_y_offset = 0.0f;
};

struct SlideSurface {
    point_graph::PointGraph<8> speed_to_force{};
    float yaw_strength = 0.0f;
    float yaw_damping = 0.0f;
};

struct SlideInput {
    V velocity{};
    V normal{};
    V side{};
    V effective_forward{};
    V reference_forward{};
    V angular_velocity{};
    float absolute_speed = 0.0f;
    float surface_speed = 0.0f;
    float slide = 0.0f;
    float elapsed = 0.0f;
    float wheel_hardness = 0.0f;
};

/// The source does not add a zero-velocity normalization fallback here.
/// Retain that branch behavior rather than manufacturing a desired direction.
V angular_correction(const SlideSettings& s, const SlideSurface& surface, SlideInput p);
///82D925E0 rejects normal velocity, measures the authored slip angle and adds
/// its signed sideways response.82E0A4E8 normalizes before its rejection again.
physics::force_queue::QueuedPointForce sliding_force(const SlideSettings& s, const SlideSurface& surface,
                                                     SlideInput p);

}  // namespace s3::player::slide_state::forces
