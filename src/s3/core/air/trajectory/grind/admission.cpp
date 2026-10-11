// Port of skate-core/src/air/trajectory/grind/admission.rs.
#include "s3/core/air/trajectory/grind/admission.hpp"

#include "s3/core/air/trajectory/launch.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::air::trajectory::grind::admission {

void apply_admitted_target(Prediction& prediction, GrindTrajectoryCandidate candidate, Vector correction,
                           Vector support_normal, Vector reference_velocity, float maximum_adjust,
                           float landing_velocity_scalar, float landing_max_angle) {
    launch::adjust_trajectory(prediction.request.trajectory, candidate.frame, correction, maximum_adjust);
    prediction.result.contact_frame = candidate.frame;
    prediction.result.contact_time = static_cast<float>(candidate.frame) * STEP;
    prediction.result.contact_normal = landing_normal(candidate.direction, reference_velocity, support_normal,
                                                      landing_velocity_scalar, landing_max_angle);
}

Vector landing_normal(Vector direction, Vector velocity, Vector support, float velocity_scalar,
                      float maximum_angle_degrees) {
    const Vector across = sub(velocity, scale(direction, dot(direction, velocity)));
    const Vector target = madd(across, -velocity_scalar, UP);
    const Vector limited = clamp_angle(target, support, maximum_angle_degrees * rs::from_bits(0x3c8efa35u));
    // 82C1E170 removes only the positive normalized-direction component,
    // multiplying by the ORIGINAL supplied direction.
    const float component = dot(limited, normalize(direction));
    return normalize(component > 0.0f ? sub(limited, scale(direction, component)) : limited);
}

Vector clamp_angle(Vector target, Vector reference, float limit) {
    const Vector a = normalize(target);
    const Vector b = normalize(reference);
    const Vector axis = cross(a, b);
    const float angle = trigonometry::acos(rs::clamp(dot(a, b), -1.0f, 1.0f));
    const float turns = rs::floor(rs::mul_add(angle, rs::from_bits(0x3e22f983u), 0.5f));
    const float wrapped = rs::mul_add(-turns, rs::from_bits(0x40c90fdbu), angle);
    if (rs::abs(wrapped) < limit || rs::from_bits(0x37800000u) > dot(axis, axis)) {
        return target;
    }
    const Vector unit = normalize(axis);
    const auto [sin, cos] = trigonometry::sin_cos(-limit * 0.5f);
    const Vector q = scale(unit, sin);
    return scale(madd(cross(q, madd(reference, cos, cross(q, reference))), 2.0f, reference), length(target));
}

std::optional<Vector> admitted_displacement(Prediction prediction, GrindTrajectoryCandidate candidate, Primitive edge,
                                            Vector reference_velocity, Vector processed_592,
                                            GrindSurfaceEvidence surface, const GrindAssistLimits& limits,
                                            std::optional<float>& effective_lock_distance) {
    const Vector natural_landing = prediction.request.trajectory.position_at(prediction.result.contact_time);
    if (candidate.point[1] - natural_landing[1] < rs::from_bits(0xbecccccdu) || candidate.direction[1] > 0.9f) {
        return std::nullopt;
    }
    const Vector direction = normalize(sub(edge.end, edge.start));
    const Vector perpendicular = sub(reference_velocity, scale(direction, dot(direction, reference_velocity)));
    if (rs::abs(perpendicular[1] * (1.0f - rs::abs(direction[1]))) > limits.max_downward_speed) {
        return std::nullopt;
    }
    if (surface.kind == 3) {
        return std::nullopt;
    }
    Vector horizontal = perpendicular;
    horizontal[1] = 0.0f;
    const float max_speed = surface.kind == 2 ? limits.max_speed_squared_ledge : limits.max_speed_squared_rail;
    if (dot(horizontal, horizontal) > max_speed) {
        return std::nullopt;
    }
    const Vector delta = sub(candidate.trajectory_point, candidate.point);
    float lock_distance;
    if (surface.kind == 2) {
        const bool incoming = dot(delta, surface.side) > 0.0f;
        const bool body = dot(sub(processed_592, edge.start), surface.side) > 0.0f;
        lock_distance = limits.lock_distance * limits.ledge_scalars[(incoming ? 0u : 2u) + (body ? 0u : 1u)];
    } else {
        lock_distance = limits.lock_distance;
    }
    lock_distance = rs::max(lock_distance, 0.1f);
    effective_lock_distance = lock_distance;
    if (candidate.distance >= lock_distance) {
        return std::nullopt;
    }
    const Vector miss = sub(candidate.trajectory_point, edge.start);
    Vector correction = scale(sub(miss, scale(candidate.direction, dot(candidate.direction, miss))), -1.0f);
    const float distance = length(correction);
    if (distance >= limits.lock_distance) {
        const float half_dimension =
            rs::mul_add(limits.deck_dimensions[1], 0.5f, limits.deck_dimensions[0] * 0.5f);
        correction = scale(correction, rs::mul_add(-limits.tip_scalar, half_dimension, distance) / distance);
    }
    const Vector original = prediction.request.trajectory.velocity;
    const Vector adjusted = madd(correction, reciprocal(candidate.time), original);
    const float angle = dot(original, original) * dot(adjusted, adjusted) > rs::from_bits(0x38d1b717u)
                            ? angle_between(original, adjusted)
                            : 0.0f;
    if (angle > limits.maximum_adjust_angle * rs::from_bits(0x3c8efa35u)) {
        return std::nullopt;
    }
    return correction;
}

}  // namespace s3::air::trajectory::grind::admission
