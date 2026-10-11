// Port of skate-core/src/physics/grind_air/pose.rs.
#include "s3/core/physics/grind_air.hpp"

namespace s3::physics::grind_air {

namespace pose {

Frame axis_rotation(V axis, float angle) {
    const auto [sin, cos] = trigonometry::sin_cos(angle);
    const float x = axis[0];
    const float y = axis[1];
    const float z = axis[2];
    const float k = 1.0f - cos;
    return Frame{{
        {rs::mul_add(k * x, x, cos), rs::mul_add(k * x, y, sin * z), rs::mul_add(k * x, z, -sin * y), 0.0f},
        {rs::mul_add(k * y, x, -sin * z), rs::mul_add(k * y, y, cos), rs::mul_add(k * y, z, sin * x), 0.0f},
        {rs::mul_add(k * z, x, sin * y), rs::mul_add(k * z, y, -sin * x), rs::mul_add(k * z, z, cos), 0.0f},
        ZERO,
    }};
}

V direction(const Frame& frame, V vector) {
    return madd(frame[2], vector[2], madd(frame[1], vector[1], scale(frame[0], vector[0])));
}

}  // namespace pose

Frame Adjustment::local_transform(PoseInput input) const {
    using pose::axis_rotation;
    using pose::direction;
    // Unlike NormalizeSafe, this native wrapper has no zero-vector fallback.
    V across = cross(UP, axis);
    across = scale(across, inverse_length(dot(across, across)));
    V up = cross(axis, across);
    up = scale(up, inverse_length(dot(up, up)));
    const Frame yaw = axis_rotation(up, angles[1]);
    const Frame lean = axis_rotation(axis, angles[0]);
    const Frame pitch = axis_rotation(input.physical_forward, angles[2]);
    Frame local = skeleton_animation_record::IDENTITY;
    for (std::size_t i = 0; i < 3; ++i) {
        local[i] = direction(input.world_to_animation,
                             direction(yaw, direction(lean, direction(pitch, input.animation_to_world[i]))));
    }
    const V pivot = input.animation_board_position;
    local[3] = sub(add(direction(input.world_to_animation, offset), pivot), direction(local, pivot));
    return local;
}

}  // namespace s3::physics::grind_air
