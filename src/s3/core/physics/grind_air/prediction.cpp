// Port of skate-core/src/physics/grind_air/prediction.rs.
#include "s3/core/physics/grind_air.hpp"

namespace s3::physics::grind_air::prediction {

Samples project(Input input, const Settings& s, V rail, V normal, std::array<float, 12>& headings) {
    Samples samples{};
    Frame board = input.board;
    V velocity = input.velocity;
    const float speed = length(input.angular_velocity);
    const Frame rotation = speed < rs::from_bits(0x37800000u)
                               ? skeleton_animation_record::IDENTITY
                               : pose::axis_rotation(scale(input.angular_velocity, 1.0f / speed),
                                                     speed * input.timestep);
    const V acceleration_step =
        madd(input.up, s.stomp * input.timestep,
             V{0.0f, input.timestep * rs::from_bits(0xc11ccccdu), 0.0f, 0.0f});
    for (std::size_t frame = 0; frame < s.frames; ++frame) {
        S3_ASSERT(frame < samples.size());
        for (std::size_t k = 0; k < 5; ++k) {
            const V& p = s.points[k];
            samples[frame][k] = madd(board[2], p[2], madd(board[1], p[1], madd(board[0], p[0], board[3])));
        }
        // 82D71F40, preserve the target's literal post-ACos 90/180 fold.
        const V forward = sub(samples[frame][1], samples[frame][4]);
        const V projected = unit(sub(forward, scale(normal, dot(normal, forward))));
        const V aligned = dot(projected, rail) < 0.0f ? scale(projected, -1.0f) : projected;
        float angle = rs::abs(trigonometry::acos(rs::min(rs::max(dot(aligned, rail), -1.0f), 1.0f)));
        if (angle > 90.0f) {
            angle = 180.0f - angle;
        }
        const float sign = dot(normal, cross(aligned, rail)) < 0.0f ? -1.0f : 1.0f;
        headings[frame] = -(sign * angle * RAD);
        for (std::size_t a = 0; a < 3; ++a) {
            board[a] = pose::direction(rotation, board[a]);
        }
        velocity = add(velocity, acceleration_step);
        board[3] = madd(velocity, input.timestep, board[3]);
    }
    return samples;
}

}  // namespace s3::physics::grind_air::prediction
