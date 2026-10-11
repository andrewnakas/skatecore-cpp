// Port of skate-core/src/air/trajectory/grind.rs.
// Original S3 trajectory grind acquisition and typed retained selection.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "s3/core/air/trajectory/grind_surface.hpp"
#include "s3/core/air/trajectory/math.hpp"
#include "s3/core/air/trajectory/prediction.hpp"
#include "s3/core/physics/grind_air.hpp"
#include "s3/core/physics/grind_contact.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::air::trajectory::grind {

using namespace trajectory::math;
using grind_surface::LandingOrientation;
using physics::grind_contact::Primitive;
using point_graph::PointGraph;
using prediction::Prediction;
using prediction::Trajectory;

struct GrindTrajectoryCandidate {
    Vector point{};
    Vector trajectory_point{};
    Vector direction{};
    Vector approach{};
    float distance = 0.0f;
    float time = 0.0f;
    float angle = 0.0f;
    std::int32_t frame = 0;
    std::size_t primitive = 0;
};

struct GrindSurfaceEvidence {
    std::uint32_t kind = 0;
    Vector side{};
};

struct GrindAssistLimits {
    float lock_distance = 0.0f;
    float max_speed_squared_ledge = 0.0f;
    float max_speed_squared_rail = 0.0f;
    float max_downward_speed = 0.0f;
    std::array<float, 4> ledge_scalars{};
    float tip_scalar = 0.0f;
    float maximum_adjust_angle = 0.0f;
    std::array<float, 2> deck_dimensions{};
};

struct GrindTarget {
    Primitive edge{};
    std::size_t provider_index = 0;
    std::uint32_t primitive_flags = 0;
    LandingOrientation orientation{};
    Vector point{};
    /// Selector2896, distinct from landing contact normal.
    Vector vertical_normal{};

    physics::grind_air::Target air_target() const {
        physics::grind_air::Target t;
        t.edge = edge;
        t.primitive_flags = primitive_flags;
        t.orientation = orientation;
        return t;
    }
};

struct GrindEvaluation {
    std::optional<GrindTarget> target;
    float score = 0.0f;
    float distance = 0.0f;
    /// 9663/9664 can be written even when candidate admission fails.
    std::optional<float> effective_lock_distance;
    float penalty_domain = 0.0f;

    float penalty_input() const {
        if (effective_lock_distance) {
            const float denominator = *effective_lock_distance + rs::from_bits(0x3e19999au);
            if (denominator < penalty_domain) {
                return (penalty_domain / denominator) * distance;
            }
        }
        return distance;
    }
};

}  // namespace s3::air::trajectory::grind

// Submodules (private `mod admission; mod candidate;`). Their `pub use` re-exports into
// `grind` are emitted at the end of each child header, so include order does not matter.
#include "s3/core/air/trajectory/grind/admission.hpp"
#include "s3/core/air/trajectory/grind/candidate.hpp"
