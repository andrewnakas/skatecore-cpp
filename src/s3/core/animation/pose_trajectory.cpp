// Port of skate-core/src/animation/pose_trajectory.rs.
#include "s3/core/animation/pose_trajectory.hpp"

#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::animation::pose_trajectory {

using rs::mul_add;

namespace {
/// Fused cross arithmetic from82D17908..17A24, without quaternion normalization.
std::array<float, 3> cross(const std::array<float, 4>& a, const std::array<float, 4>& b) {
    return {
        mul_add(-a[2], b[1], a[1] * b[2]),
        mul_add(-a[0], b[2], a[2] * b[0]),
        mul_add(-a[1], b[0], a[0] * b[1]),
    };
}
}  // namespace

Sqt delta(Sqt current, Sqt previous, std::optional<LoopTransform> loop_transform_opt) {
    const LoopTransform loop_transform =
        loop_transform_opt.value_or(LoopTransform{{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}});
    const std::array<float, 4> inverse_previous = {
        -previous.rotation[0],
        -previous.rotation[1],
        -previous.rotation[2],
        previous.rotation[3],
    };
    const std::array<float, 3> loop_displacement = rotate(current.rotation, loop_transform.translation);
    std::array<float, 3> displacement{};
    for (std::size_t i = 0; i < 3; ++i) {
        displacement[i] = (loop_displacement[i] + current.translation[i]) - previous.translation[i];
    }
    const std::array<float, 3> translation = rotate(inverse_previous, displacement);
    const std::array<float, 4> rotation = multiply(multiply(loop_transform.rotation, current.rotation), inverse_previous);
    Sqt out;
    out.scale = current.scale;
    out.rotation = rotation;
    // The extractor restores the channel weight separately for channel clips.
    out.translation = {translation[0], translation[1], translation[2], current.translation[3]};
    return out;
}

std::array<float, 3> rotate(std::array<float, 4> q, std::array<float, 3> v) {
    const std::array<float, 4> vector = {v[0], v[1], v[2], 0.0f};
    const std::array<float, 3> first = cross(q, vector);
    std::array<float, 4> intermediate{};
    for (std::size_t i = 0; i < 4; ++i) intermediate[i] = i < 3 ? mul_add(q[3], v[i], first[i]) : 0.0f;
    const std::array<float, 3> second = cross(q, intermediate);
    return {mul_add(2.0f, second[0], v[0]), mul_add(2.0f, second[1], v[1]), mul_add(2.0f, second[2], v[2])};
}

std::array<float, 4> multiply(std::array<float, 4> a, std::array<float, 4> b) {
    const std::array<float, 3> vector = cross(a, b);
    return {
        mul_add(a[0], b[3], mul_add(b[0], a[3], vector[0])),
        mul_add(a[1], b[3], mul_add(b[1], a[3], vector[1])),
        mul_add(a[2], b[3], mul_add(b[2], a[3], vector[2])),
        a[3] * b[3] - physics::native_arithmetic::dot3(a, b),
    };
}

}  // namespace s3::animation::pose_trajectory
