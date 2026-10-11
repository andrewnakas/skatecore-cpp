// Port of skate-core/src/air/trajectory/query.rs.
// Complete trajectory-segment walk82770910 and result writer8276DBE0.
// The host supplies real nearest swept-line hits and intersecting triangles.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "s3/core/air/trajectory/math.hpp"
#include "s3/core/air/trajectory/prediction.hpp"

namespace s3::air::trajectory::query {

using namespace trajectory::math;
using prediction::QueryRequest;
using prediction::QueryResult;

struct SurfaceHit {
    Vector position{};
    Vector normal{};
    Transform transform{};
    std::uint32_t surface = 0;
    std::uint32_t geometry = 0;
};

// ---- module-private helpers (query.rs) ----
inline float reciprocal(float value) {
    float inverse = physics::native_arithmetic::reciprocal_estimate(value);
    for (int i = 0; i < 2; ++i) {
        inverse = rs::mul_add(inverse, rs::mul_add(-inverse, value, 1.0f), inverse);
    }
    return inverse;
}
inline float step_size(float gravity_y, float error_radius) {
    const float square = reciprocal(gravity_y) * (-8.0f * error_radius);
    return square == 0.0f ? 0.0f : square * inverse_length(square);
}
/// 8276C558 writes both collision seconds and integer frame, including its
/// distinct negligible-gravity branch. A miss is handled by the caller.
std::pair<float, std::int32_t> time_at_contact(QueryRequest request, Vector position, float time);
/// 82771018, using at most64 triangles retained by82772028 in world order.
Vector average_landing_normal(const std::vector<std::array<Vector, 3>>& triangles, Vector velocity);

/// `line(start, end, radius)` returns rs::Result<std::optional<SurfaceHit>, E>;
/// `nearby(position, radius)` returns rs::Result<std::vector<std::array<Vector, 3>>, E>.
template <class E, class Line, class Nearby>
rs::Result<QueryResult, E> query_trajectory(QueryRequest request, Line&& line, Nearby&& nearby) {
    const auto trajectory = request.trajectory;
    float start_step = trajectory.duration;
    float end_step = trajectory.duration;
    if (trajectory.acceleration[1] < 0.0f) {
        start_step = step_size(trajectory.acceleration[1], request.start_error * request.radius);
        end_step = step_size(trajectory.acceleration[1], request.end_error * request.radius);
    }
    const float minimum_square = ((request.start_error * request.radius) * request.start_error) * request.radius;
    float time = 0.0f;
    Vector start = trajectory.position_at(time);
    Vector end = trajectory.position_at(time + start_step);
    while (trajectory.duration > time) {
        const Vector delta = sub(end, start);
        if (dot(delta, delta) > minimum_square) {
            // 82770B40 rejects a segment only when all XYZ are tiny.
            const float tiny = rs::from_bits(0x37800000u);
            if (rs::abs(delta[0]) > tiny || rs::abs(delta[1]) > tiny || rs::abs(delta[2]) > tiny) {
                rs::Result<std::optional<SurfaceHit>, E> line_result = line(start, end, request.radius);
                if (line_result.is_err()) return rs::make_err(line_result.unwrap_err());
                const std::optional<SurfaceHit> hit = line_result.unwrap();
                if (hit) {
                    const auto [contact_time, contact_frame] = time_at_contact(request, hit->position, time);
                    rs::Result<std::vector<std::array<Vector, 3>>, E> nearby_result =
                        nearby(hit->position, request.radius);
                    if (nearby_result.is_err()) return rs::make_err(nearby_result.unwrap_err());
                    const auto& triangles = nearby_result.unwrap();
                    const Vector velocity = trajectory.velocity_at(static_cast<float>(contact_frame) * STEP);
                    QueryResult out{};
                    out.contact_position = hit->position;
                    out.contact_normal = hit->normal;
                    out.landing_normal = average_landing_normal(triangles, velocity);
                    out.contact_time = contact_time;
                    out.contact_frame = contact_frame;
                    out.contact_transform = hit->transform;
                    out.surface = hit->surface;
                    out.geometry = hit->geometry;
                    return out;
                }
            }
            start = end;
        }
        const float fraction = reciprocal(trajectory.duration) * time;
        const float step = rs::mul_add(end_step, fraction, (1.0f - fraction) * start_step);
        time += step;
        // The native walk evaluates beyond the horizon; it does not clamp end.
        end = trajectory.position_at(time + step);
    }
    return QueryResult::miss();
}

}  // namespace s3::air::trajectory::query
