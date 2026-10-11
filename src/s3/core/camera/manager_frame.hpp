// Port of skate-core/src/camera/manager_frame.rs.
//! Final CameraMan frame arithmetic82DFF88C..82DFFF28, Euler82DFCD00.
#pragma once
#include "s3/core/camera/_types.hpp"
#include "s3/core/math.hpp"

namespace s3::camera {

struct CameraFrame {
    math::Basis3 basis;
    F4 position{};
    math::Basis3 previous_basis;
    F4 previous_position{};
    F4 linear_velocity{};
    F4 angular_velocity{};
    F4 shake_translation{};
    bool discontinuity = false;
    float field_of_view_degrees = 0.0f;
    float opacity = 0.0f;
    float blur = 0.0f;

    /// pub(super)
    static CameraFrame new_();
    /// pub(super)
    void motion(float dt, const math::Basis3& basis, F4 position);
};
S3_DERIVE_EQ(CameraFrame, basis, position, previous_basis, previous_position, linear_velocity, angular_velocity,
             shake_translation, discontinuity, field_of_view_degrees, opacity, blur)

namespace manager_frame {
/// pub(super)
float lens_field_of_view(float lens, float aspect);
}  // namespace manager_frame

}  // namespace s3::camera
