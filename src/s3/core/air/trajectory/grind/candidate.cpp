// Port of skate-core/src/air/trajectory/grind/candidate.rs.
#include "s3/core/air/trajectory/grind/candidate.hpp"

namespace s3::air::trajectory::grind::candidate {

std::vector<std::size_t> trajectory_box_filter(const std::vector<std::size_t>& indices,
                                               const std::vector<Primitive>& primitives, Vector takeoff) {
    std::vector<Vector> directions;
    directions.reserve(indices.size());
    for (const std::size_t i : indices) {
        const Vector d = sub(primitives[i].end, primitives[i].start);
        directions.push_back(scale(d, inverse_length(dot(d, d))));
    }
    std::vector<std::size_t> accepted;
    accepted.reserve(indices.size());
    for (std::size_t i = 0; i < indices.size(); ++i) {
        const Primitive a = primitives[indices[i]];
        bool keep = true;
        for (std::size_t j = 0; j < indices.size(); ++j) {
            if (i == j) {
                continue;
            }
            const Vector crossed = cross(directions[i], directions[j]);
            if (!(dot(crossed, crossed) < rs::from_bits(0x38d1b717u))) {
                continue;
            }
            const Primitive b = primitives[indices[j]];
            const Vector delta = sub(b.start, a.start);
            const Vector perpendicular = sub(delta, scale(directions[j], dot(directions[j], delta)));
            const float distance = dot(perpendicular, perpendicular);
            if (!(distance > rs::from_bits(0x38d1b717u) && distance < rs::from_bits(0x3c23d70bu))) {
                continue;
            }
            const Vector middle = scale(add(a.start, a.end), 0.5f);
            const Vector from = sub(b.start, middle);
            if (!(dot(from, sub(b.end, middle)) < 0.0f)) {
                continue;
            }
            const Vector up = normalize(cross(cross(directions[i], UP), directions[i]));
            const float height = dot(delta, up);
            const Vector alternative = add(middle, sub(from, scale(directions[j], dot(directions[j], from))));
            const Vector first = sub(middle, takeoff);
            const Vector second = sub(alternative, takeoff);
            if ((height > -0.02f && dot(first, first) > dot(second, second)) || height > 0.02f) {
                keep = false;
            }
        }
        if (keep) {
            accepted.push_back(indices[i]);
        }
    }
    return accepted;
}

std::optional<float> descending_plane_time(Trajectory t, Vector point, Vector normal) {
    const float c = dot(normal, t.position) - dot(point, normal);
    const float b = dot(normal, t.velocity);
    const float a = dot(normal, scale(t.acceleration, reciprocal(2.0f)));
    const float discriminant = b * b - (4.0f * a) * c;
    if (discriminant < 0.0f) {
        return std::nullopt;
    }
    const float inverse = reciprocal(2.0f * a);
    if (discriminant > 0.0f) {
        const float root = discriminant * inverse_length(discriminant);
        return rs::max(inverse * (-b + root), inverse * (-b - root));
    }
    const float time = inverse * -b;
    if (time > 0.0f) return time;
    return std::nullopt;
}

std::optional<GrindTrajectoryCandidate> consider_grind_primitive(Prediction prediction, Primitive edge,
                                                                 std::size_t primitive, float padding) {
    Vector delta = sub(edge.end, edge.start);
    const Vector normal_unscaled = cross(delta, cross(UP, delta));
    const Vector normal = scale(normal_unscaled, inverse_length(dot(normal_unscaled, normal_unscaled)));
    const float rail_length = length(delta);
    // 830BD320 is initialized to1 by82F825D0. The original endpoints remain
    // unchanged; only the direction and endpoint overrun vector are capped.
    if (rail_length > 1.0f) {
        delta = scale(delta, reciprocal(rail_length));
    }
    const Vector offset = madd(normal, prediction.request.radius, scale(normal, padding));
    const std::optional<float> time_opt =
        descending_plane_time(prediction.request.trajectory, add(edge.start, offset), normal);
    if (!time_opt) {
        return std::nullopt;
    }
    const float time = *time_opt;
    const Vector trajectory_point = sub(prediction.request.trajectory.position_at(time), offset);
    const Vector direction = normalize(delta);
    const Vector difference = sub(trajectory_point, edge.start);
    const float distance = length(cross(difference, direction));
    const Vector point = madd(direction, dot(direction, difference), edge.start);
    const Vector extension = scale(delta, 0.1f);
    // Strict endpoint test82D6A708. No closest-point clamping.
    if (!(dot(sub(point, sub(edge.start, extension)), sub(point, add(edge.end, extension))) < 0.0f)) {
        return std::nullopt;
    }
    const Vector velocity = prediction.request.trajectory.velocity;
    const Vector approach = normalize(sub(velocity, scale(normal, dot(normal, velocity))));
    Vector horizontal = sub(point, prediction.request.trajectory.position);
    horizontal[1] = 0.0f;
    const float angle = folded_approach_angle(horizontal, direction, normal);
    GrindTrajectoryCandidate c;
    c.point = point;
    c.trajectory_point = trajectory_point;
    c.direction = direction;
    c.approach = approach;
    c.distance = distance;
    c.time = time;
    c.angle = angle;
    c.frame = rs::as_i32(time * rs::from_bits(0x426fffffu));
    c.primitive = primitive;
    return c;
}

float folded_approach_angle(Vector a, Vector b, Vector normal) {
    float angle = angle_between(a, b);
    const float aa = dot(a, a);
    const float bb = dot(b, b);
    if (aa > rs::from_bits(0x38d1b717u) && bb > rs::from_bits(0x38d1b717u)) {
        auto unit = [](Vector v, float square) {
            const float r = physics::native_arithmetic::reciprocal_square_root_estimate(square);
            return scale(v, rs::mul_add(r * 0.5f, rs::mul_add(-square, r * r, 1.0f), r));
        };
        if (dot(cross(unit(a, aa), unit(b, bb)), normal) < 0.0f) {
            angle = rs::from_bits(0x40c90fdbu) - angle;
        }
    }
    const float turns = angle * rs::from_bits(0x3e22f983u);
    const float fraction = turns - rs::floor(turns);
    const float wrapped = (fraction - (fraction > 0.5f ? 1.0f : 0.0f)) * rs::from_bits(0x40c90fdbu);
    const float sign = wrapped > 0.0f ? 1.0f : -1.0f;
    const float magnitude = wrapped * sign;
    const float folded = magnitude > rs::from_bits(0x3fc90fdbu) ? magnitude - rs::f32::consts::PI : magnitude;
    return rs::abs(sign * folded);
}

std::optional<GrindTrajectoryCandidate> take_best_grind(std::vector<GrindTrajectoryCandidate>& candidates,
                                                        float difficulty_distance,
                                                        const PointGraph<8>& height_penalty) {
    std::optional<std::size_t> chosen;
    float fallback = 1000.0f;
    float best_angle = rs::f32::consts::PI;
    float height = -10000.0f;
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        const GrindTrajectoryCandidate& candidate = candidates[i];
        if (candidate.distance < difficulty_distance) {
            const float benefit = best_angle - candidate.angle;
            if (benefit > height_penalty.evaluate(candidate.point[1] - height)) {
                height = candidate.point[1];
                fallback = -1.0f;
                best_angle = candidate.angle;
                chosen = i;
            }
        } else if (candidate.distance < fallback) {
            fallback = candidate.distance;
            chosen = i;
        }
    }
    if (!chosen) {
        return std::nullopt;
    }
    const GrindTrajectoryCandidate removed = candidates[*chosen];
    candidates.erase(candidates.begin() + static_cast<std::ptrdiff_t>(*chosen));
    return removed;
}

}  // namespace s3::air::trajectory::grind::candidate
