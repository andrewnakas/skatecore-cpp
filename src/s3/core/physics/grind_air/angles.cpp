// Port of skate-core/src/physics/grind_air/angles.rs.
#include "s3/core/physics/grind_air.hpp"

namespace s3::physics::grind_air::angles {

void update(GrindAir& state, Input input, const Settings& s, Target target,
            const prediction::Samples& samples, V rail, V normal, std::size_t kind, float time) {
    // 82D723D0 stores angleArray[type], NOT the range validator's frame index.
    const float heading = state.headings[kind];
    const float desired = heading > 0.0f ? targets::DESIRED[kind] : -targets::DESIRED[kind];
    const float speed =
        rs::min(rs::max((desired - heading) / time * s.yaw_assist[kind], -s.max_angle), s.max_angle);
    state.angle_delta[1] =
        rs::min(rs::max(state.angle_delta[1] * RAD + speed, -s.max_angle), s.max_angle) * DEG;
    const std::size_t index = rs::as_usize(time + 0.5f);
    S3_ASSERT(index < samples.size());
    const auto& points = samples[index];
    const V forward = sub(points[1], points[4]);
    const V projected = unit(sub(forward, scale(rail, dot(rail, forward))));
    const LandingOrientation o = target.orientation;
    float error;
    switch (kind) {
        case 0:
        case 3: {
            const float angle = signed_angle(projected, o.boardslide_dir, rail);
            if (angle > PI * 0.5f) {
                error = angle - PI;
            } else if (angle < -PI * 0.5f) {
                error = angle + PI;
            } else {
                error = angle;
            }
            break;
        }
        case 1:
        case 2: {
            const V from = kind == 1 ? scale(projected, -1.0f) : projected;
            const float a = signed_angle(from, o.tipslide_dir, rail);
            const float b = signed_angle(from, o.backslash_dir, rail);
            error = rs::abs(a) < rs::abs(b) ? a : b;
            break;
        }
        default:
            error = 0.0f;
            break;
    }
    const float maximum = s.max_angle * DEG;
    state.angle_delta[0] = rs::min(rs::max(state.angle_delta[0] + error / time, -maximum), maximum);
    // 82D71BB0 native broadcast subtraction is intentional, not vector rejection.
    const float d = dot(input.board[0], normal);
    const V projected_right =
        unit(V{input.board[0][0] - d, input.board[0][1] - d, input.board[0][2] - d, input.board[0][3] - d});
    const float correction = dot(projected_right, projected_right) > 0.9f
                                 ? signed_angle(input.board[0], projected_right, input.board[2]) * 0.9f
                                 : 0.0f;
    state.angle_delta[2] = kind < 4 ? correction / time : 0.0f;
    state.angles = add(state.angles, state.angle_delta);
}

}  // namespace s3::physics::grind_air::angles
