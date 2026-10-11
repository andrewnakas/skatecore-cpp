// Port of skate-core/src/air/trajectory/launch.rs.
#include "s3/core/air/trajectory/launch.hpp"

#include <algorithm>

#include "s3/core/trigonometry.hpp"

namespace s3::air::trajectory::launch {

using namespace trajectory::math;

bool adjust_velocity(LaunchInfo& info, SelectorInput input, const SelectorSettings& s) {
    const Vector n = input.ground_normal;
    const Vector v = info.start_velocity;
    Vector normal = scale(n, dot(v, n));
    normal[1] = 0.0f;
    const Vector combined = add(Vector{0.0f, v[1], 0.0f, 0.0f}, normal);
    const Vector residual = sub(v, combined);
    const float lean = rs::min(rs::max(input.directional_input - 0.25f, -1.0f), 1.0f);
    const Vector aligned = normalize(sub(UP, scale(n, lean * s.vert_jump_align_max_angle)));
    if (n[1] >= s.vert_jump_align_max_ground_normal_y) {
        return false;
    }
    if (normalize(combined)[1] > s.vert_jump_align_min_direction_y) {
        // The second term really uses UNIT velocity, not the original speed.
        const Vector left = scale(add(scale(aligned, length(combined)), residual), s.vert_jump_align_factor);
        const Vector right = scale(normalize(v), 1.0f - s.vert_jump_align_factor);
        info.start_velocity = scale(normalize(add(left, right)), length(v));
        return true;
    }
    if (!info.player_jumped && input.previous_physics_state != 200) {
        const Vector normal2 = scale(n, dot(v, n));
        info.start_velocity = add(scale(normal2, s.natural_air_off_verts_scalar), sub(v, normal2));
    }
    return false;
}

std::vector<Vector> candidate_velocities(LaunchInfo info, const SelectorSettings& s) {
    const std::size_t count = std::min<std::uint16_t>(info.trajectory_count, 7);
    std::vector<Vector> velocities;
    velocities.reserve(count);
    if (count == 0) {
        return velocities;
    }
    const Vector v = info.start_velocity;
    velocities.push_back(v);
    Vector right = normalize(cross(UP, v));
    Vector forward = normalize(cross(right, UP));
    const float speed = length(v);
    float cone_speed = info.player_jumped ? length(info.com_velocity) : speed;
    const float radians = rs::from_bits(0x3c8efa35u);
    const float x = info.cone_angle_x * radians;
    const float z = (s.cone_angle_z_vs_speed.evaluate(cone_speed * 0.05f) * info.cone_angle_z) * radians;
    cone_speed = rs::min(rs::max(cone_speed, s.speed_factor_min), s.speed_factor_max);
    right = scale(right, trigonometry::sin(x) * cone_speed);
    forward = scale(forward, trigonometry::sin(z) * cone_speed);
    if (count > 1) {
        const float step = rs::from_bits(0x40c90fdbu) / static_cast<float>(count - 1);
        float angle = 0.0f;
        for (std::size_t i = 1; i < count; ++i) {
            const Vector candidate = madd(forward, trigonometry::cos(angle), madd(right, trigonometry::sin(angle), v));
            velocities.push_back(scale(normalize(candidate), rs::min(length(candidate), speed)));
            angle += step;
        }
    }
    return velocities;
}

LaunchBatch batch(LaunchInfo info, SelectorInput input, const SelectorSettings& s) {
    std::vector<Vector> velocities = candidate_velocities(info, s);
    const float ground = s.displacement_vs_ground_normal.evaluate(rs::abs(input.ground_normal[1]));
    const float speed = s.displacement_vs_speed.evaluate(input.board_vertical_velocity * 0.1f);
    const float blend = rs::max(speed, ground);
    const float height =
        dot(sub(info.animation_com_position, input.contact_position), info.reckoning_transform[1]) - s.trajectory_radius;
    const float displacement =
        rs::min(height, s.trajectory_displacement) * (1.0f - blend) + s.trajectory_displacement * blend;
    Vector origin = sub(info.animation_com_position, scale(info.reckoning_transform[1], displacement));
    Vector board_position = info.board_position;
    if (info.use_position_override) {
        origin = info.start_position_override;
        board_position = info.board_position_override;
    }
    const Vector com_displacement = sub(info.animation_com_position, origin);
    const float duration = max_time(info.start_velocity, s);
    std::vector<QueryRequest> requests;
    requests.reserve(velocities.size());
    for (const Vector& velocity : velocities) {
        QueryRequest r{};
        r.trajectory = Trajectory{madd(velocity, info.timestep, origin), velocity, input.gravity, duration};
        r.radius = s.trajectory_radius;
        r.start_error = s.trajectory_error_start;
        r.end_error = s.trajectory_error_end;
        requests.push_back(r);
    }
    LaunchBatch out;
    out.requests = std::move(requests);
    out.velocities = std::move(velocities);
    out.origin = origin;
    out.board_position = board_position;
    out.com_displacement = com_displacement;
    out.local_board_position = transform(info.reckoning_inverse, sub(info.board_position, origin));
    out.local_com_position = transform(info.reckoning_inverse, com_displacement);
    return out;
}

float max_time(Vector velocity, const SelectorSettings& s) {
    const float square =
        rs::mul_add(velocity[1], velocity[1], -(s.trajectory_max_drop * rs::from_bits(0xc19ccccdu)));
    const float root = square == 0.0f ? 0.0f : square * inverse_length(square);
    return rs::min((-velocity[1] - root) * rs::from_bits(0xbdd0fac6u), s.trajectory_max_time);
}

void adjust_trajectory(Trajectory& trajectory, std::int32_t frame, Vector adjustment, float maximum) {
    if (frame <= 0) {
        return;
    }
    const Vector correction = scale(adjustment, reciprocal(static_cast<float>(frame) * STEP));
    const float scalar =
        dot(correction, correction) > maximum * maximum ? maximum / length(correction) : 1.0f;
    trajectory.velocity = madd(correction, scalar, trajectory.velocity);
}

}  // namespace s3::air::trajectory::launch
