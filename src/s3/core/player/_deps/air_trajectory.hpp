// TEMP DEP (stage P2): replace with stage A/P1 header `s3/core/air/trajectory.hpp`.
// Minimal faithful port of skate-core/src/air/trajectory/prediction.rs (the only
// trajectory items player/** uses: Trajectory, QueryRequest, QueryResult, Prediction).
// Used automatically only while the stage-P1 header is missing.
#pragma once
#if __has_include("s3/core/air/trajectory.hpp")
#include "s3/core/air/trajectory.hpp"
#else
#include "s3/core/air/trajectory/math.hpp"

namespace s3::air::trajectory {

using math::Transform;
using math::Vector;

struct Trajectory {
    Vector position{};      // 0 metres
    Vector velocity{};      // 16 metres/second
    Vector acceleration{};  // 32 metres/second squared
    float duration = 0.0f;  // 48 seconds; -1 means unbounded

    Vector position_at(float time) const {
        const float square = time * time;
        return rs::from_fn<4>([&](std::size_t i) {
            return rs::mul_add(acceleration[i] * 0.5f, square, rs::mul_add(velocity[i], time, position[i]));
        });
    }
    Vector velocity_at(float time) const {
        return rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(acceleration[i], time, velocity[i]); });
    }
    std::pair<Vector, float> highest_position() const {
        // 82D2CCA0: downward velocity or nonnegative gravity has no apex.
        if (velocity[1] < 0.0f || acceleration[1] >= 0.0f) {
            return {position, 0.0f};
        }
        const float t = velocity[1] * math::reciprocal(-acceleration[1]);
        return {position_at(t), t};
    }
};
S3_DERIVE_EQ(Trajectory, position, velocity, acceleration, duration)

struct QueryRequest {
    Trajectory trajectory{};  // request144..207
    float radius = 0.0f;       // 208 metres
    float start_error = 0.0f;  // 212 multiplier of radius
    float end_error = 0.0f;    // 216 multiplier of radius
};
S3_DERIVE_EQ(QueryRequest, trajectory, radius, start_error, end_error)

struct QueryResult {
    Vector contact_position{};    // 0
    Vector contact_normal{};      // 16 first hit face normal
    Vector landing_normal{};      // 32 velocity-qualified nearby-face average
    float contact_time = 0.0f;    // 48 seconds; -1 is no contact
    Transform contact_transform{};  // 64 static world identity
    std::int32_t contact_frame = 0;  // 128
    std::uint32_t surface = 0;       // 132 original surface data
    std::uint32_t geometry = 0;      // 136 host geometry identity

    static QueryResult miss() {
        QueryResult r;
        r.contact_position = math::ZERO;
        r.contact_normal = math::UP;
        r.landing_normal = math::UP;
        r.contact_time = -1.0f;
        r.contact_transform = math::IDENTITY;
        r.contact_frame = -1;
        r.surface = 0;
        r.geometry = 0;
        return r;
    }
    bool valid() const { return contact_time >= 0.0f; }
    /// PredictionResults::GetLandingNormal82D2D9E8.
    Vector suggested_normal() const { return valid() ? landing_normal : math::UP; }
};
S3_DERIVE_EQ(QueryResult, contact_position, contact_normal, landing_normal, contact_time, contact_transform,
             contact_frame, surface, geometry)

struct Prediction {
    QueryResult result{};
    QueryRequest request{};

    Vector collision_position() const { return request.trajectory.position_at(result.contact_time); }
    Vector collision_velocity() const {
        return request.trajectory.velocity_at(static_cast<float>(result.contact_frame) * math::STEP);
    }
};
S3_DERIVE_EQ(Prediction, result, request)

}  // namespace s3::air::trajectory
#endif
