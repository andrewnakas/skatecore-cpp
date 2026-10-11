// Port of skate-core/src/air/trajectory/query.rs.
#include "s3/core/air/trajectory/query.hpp"

namespace s3::air::trajectory::query {

std::pair<float, std::int32_t> time_at_contact(QueryRequest request, Vector position, float time) {
    const float EPSILON = rs::from_bits(0x38d1b717u);
    const auto trajectory = request.trajectory;
    if (EPSILON >= rs::abs(reciprocal(2.0f) * trajectory.acceleration[1])) {
        const float distance = length(sub(position, trajectory.position));
        if (!(rs::abs(distance) > EPSILON)) {
            return {0.0f, 0};
        }
        const float speed = length(trajectory.velocity);
        if (!(rs::abs(speed) > EPSILON)) {
            return {0.0f, 0};
        }
        const float frames = reciprocal(speed) * distance;
        return {frames * STEP, rs::as_i32(rs::ceil(frames))};
    }
    float closest_square = rs::f32::MAX;
    bool found = false;
    while (trajectory.duration >= time) {
        const Vector delta = sub(position, trajectory.position_at(time));
        const float square = dot(delta, delta);
        if (closest_square >= square) {
            closest_square = square;
        } else if (square > closest_square) {
            time -= STEP;
            found = true;
            break;
        }
        time += STEP;
    }
    if (!found) {
        time = trajectory.duration;
    }
    // 82F4E948 is ceiling (truncate then add1 for a positive remainder).
    return {time, rs::as_i32(rs::ceil(reciprocal(STEP) * time))};
}

Vector average_landing_normal(const std::vector<std::array<Vector, 3>>& triangles, Vector velocity) {
    std::vector<Vector> accepted;
    accepted.reserve(64);
    Vector most_up = ZERO;
    float highest = -1.0f;
    const std::size_t count = std::min<std::size_t>(triangles.size(), 64);
    for (std::size_t t = 0; t < count; ++t) {
        const auto& triangle = triangles[t];
        Vector normal = cross(sub(triangle[1], triangle[0]), sub(triangle[2], triangle[0]));
        // This first stage uses Normalize, not NormalizeSafe.
        normal = scale(normal, inverse_length(dot(normal, normal)));
        if (normal[1] > 0.7f || dot(normal, velocity) <= 0.0f) {
            const float up = dot(normal, UP);
            if (up > highest) {
                highest = up;
                most_up = normal;
            }
            accepted.push_back(normal);
        }
    }
    if (accepted.empty()) {
        return UP;
    }
    Vector total = ZERO;
    for (const Vector& normal : accepted) {
        if (dot(most_up, normal) > 0.5f) {
            total = add(total, normal);
        }
    }
    return normalize(total);
}

}  // namespace s3::air::trajectory::query
