// Port of skate-game/src/animation.rs.
//! SIMREF: only `native_matrix` from skate-game/src/animation.rs (the rest is Bevy skinning/render).
#pragma once
#include "s3/core/animation/output/native.hpp"
#include "s3/game/animation_pose/glam.hpp"

namespace s3::game::animation {

inline glam::Mat4 native_matrix(const s3::animation::output::NativeMatrix& matrix) {
    return glam::Mat4::from_cols(
        glam::Vec3::from_array({matrix[0][0], matrix[0][1], matrix[0][2]}).extend(0.0f),
        glam::Vec3::from_array({matrix[1][0], matrix[1][1], matrix[1][2]}).extend(0.0f),
        glam::Vec3::from_array({matrix[2][0], matrix[2][1], matrix[2][2]}).extend(0.0f),
        glam::Vec3::from_array({matrix[3][0], matrix[3][1], matrix[3][2]}).extend(1.0f));
}

}  // namespace s3::game::animation
