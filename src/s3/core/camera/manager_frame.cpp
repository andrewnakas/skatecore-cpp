// Port of skate-core/src/camera/manager_frame.rs.
#include "s3/core/camera/manager_frame.hpp"

#include <array>

#include "s3/core/camera/angle_tracker.hpp"
#include "s3/core/camera/manager_incline.hpp"
#include "s3/core/input/angle.hpp"
#include "s3/core/physics/reciprocal_sqrt.hpp"

namespace s3::camera {
namespace {

float square_root(float value) {
    float inverse = physics::reciprocal_sqrt::estimate(value);
    for (int k = 0; k < 2; ++k) {
        inverse = rs::mul_add(inverse * 0.5f, rs::mul_add(-value, inverse * inverse, 1.0f), inverse);
    }
    return value == 0.0f ? 0.0f : value * inverse;
}

std::array<float, 3> angles(const math::Basis3& basis) {
    const auto& right = basis.columns[0];
    const auto& up = basis.columns[1];
    const auto& at = basis.columns[2];
    const float horizontal = square_root(rs::mul_add(right[0], right[0], right[1] * right[1]));
    const float yaw = manager_incline::atan_ratio(-right[2], horizontal);
    if (horizontal > rs::from_bits(0x3a83126f)) {
        return {manager_incline::atan_ratio(up[2], at[2]), yaw, manager_incline::atan_ratio(right[1], right[0])};
    }
    return {manager_incline::atan_ratio(-at[1], up[1]), yaw, 0.0f};
}

}  // namespace

CameraFrame CameraFrame::new_() {
    math::Basis3 basis;
    basis.columns = {{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
    CameraFrame f;
    f.basis = basis;
    f.position = F4{};
    f.previous_basis = basis;
    f.previous_position = F4{};
    f.linear_velocity = F4{};
    f.angular_velocity = F4{};
    f.shake_translation = F4{};
    f.discontinuity = false;
    f.field_of_view_degrees = 0.0f;
    f.opacity = 1.0f;
    f.blur = 1.0f;
    return f;
}

void CameraFrame::motion(float dt, const math::Basis3& basis_, F4 position_) {
    const float inverse = dt != 0.0f ? 1.0f / dt : 1.0f;
    this->linear_velocity = rs::from_fn<4>([&](std::size_t i) { return (position_[i] - this->position[i]) * inverse; });
    const std::array<float, 3> old = angles(this->basis);
    const std::array<float, 3> new_angles = angles(basis_);
    this->angular_velocity = rs::from_fn<4>([&](std::size_t i) {
        if (i == 3) {
            return 0.0f;
        }
        return normalize_angle(new_angles[i] - old[i]) * inverse;
    });
}

namespace manager_frame {
float lens_field_of_view(float lens, float aspect) {
    const float angle = input::angle::atan(35.0f / (lens * 2.0f));
    const float degrees = angle * rs::from_bits(0x42e52ee1);
    return square_root((degrees * degrees) / (aspect + 1.0f));
}
}  // namespace manager_frame

}  // namespace s3::camera
