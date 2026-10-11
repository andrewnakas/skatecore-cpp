// Port of skate-core/src/air/trajectory/prediction.rs.
// tTrajectory and complete PredictionResults8276C9D8/8276DBE0.
#pragma once
#include <cstdint>
#include <utility>

#include "s3/core/air/trajectory/math.hpp"

namespace s3::air::trajectory::prediction {

using math::IDENTITY;
using math::reciprocal;
using math::STEP;
using math::Transform;
using math::UP;
using math::Vector;
using math::ZERO;

struct Trajectory {
    Vector position{};      // 0 metres
    Vector velocity{};      // 16 metres/second
    Vector acceleration{};  // 32 metres/second squared
    float duration = 0.0f;  // 48 seconds; -1 means unbounded

    Vector position_at(float time) const {
        const float square = time * time;
        Vector out{};
        for (std::size_t i = 0; i < 4; ++i) {
            out[i] = rs::mul_add(acceleration[i] * 0.5f, square, rs::mul_add(velocity[i], time, position[i]));
        }
        return out;
    }
    Vector velocity_at(float time) const {
        Vector out{};
        for (std::size_t i = 0; i < 4; ++i) out[i] = rs::mul_add(acceleration[i], time, velocity[i]);
        return out;
    }
    std::pair<Vector, float> highest_position() const {
        // 82D2CCA0: downward velocity or nonnegative gravity has no apex.
        if (velocity[1] < 0.0f || acceleration[1] >= 0.0f) {
            return {position, 0.0f};
        }
        const float t = velocity[1] * reciprocal(-acceleration[1]);
        return {position_at(t), t};
    }
};
S3_DERIVE_EQ(Trajectory, position, velocity, acceleration, duration)

struct QueryRequest {
    Trajectory trajectory{};  // request144..207
    float radius = 0.0f;      // 208 metres
    float start_error = 0.0f;  // 212 multiplier of radius
    float end_error = 0.0f;    // 216 multiplier of radius
};
S3_DERIVE_EQ(QueryRequest, trajectory, radius, start_error, end_error)

struct QueryResult {
    Vector contact_position{};     // 0
    Vector contact_normal{};       // 16 first hit face normal
    Vector landing_normal{};       // 32 velocity-qualified nearby-face average
    float contact_time = 0.0f;     // 48 seconds; -1 is no contact
    Transform contact_transform{};  // 64 static world identity
    std::int32_t contact_frame = 0;  // 128
    std::uint32_t surface = 0;       // 132 original surface data
    std::uint32_t geometry = 0;      // 136 host geometry identity

    static constexpr QueryResult miss() {
        QueryResult r{};
        r.contact_position = ZERO;
        r.contact_normal = UP;
        r.landing_normal = UP;
        r.contact_time = -1.0f;
        r.contact_transform = IDENTITY;
        r.contact_frame = -1;
        r.surface = 0;
        r.geometry = 0;
        return r;
    }
    bool valid() const { return contact_time >= 0.0f; }
    /// PredictionResults::GetLandingNormal82D2D9E8.
    Vector suggested_normal() const { return valid() ? landing_normal : UP; }
};
S3_DERIVE_EQ(QueryResult, contact_position, contact_normal, landing_normal, contact_time, contact_transform,
             contact_frame, surface, geometry)

struct Prediction {
    QueryResult result{};
    QueryRequest request{};

    Vector collision_position() const { return request.trajectory.position_at(result.contact_time); }
    Vector collision_velocity() const {
        return request.trajectory.velocity_at(static_cast<float>(result.contact_frame) * STEP);
    }
};
S3_DERIVE_EQ(Prediction, result, request)

}  // namespace s3::air::trajectory::prediction
