// Port of skate-game/src/transform_shim.rs
//! SIMREF: minimal copy of bevy_transform 0.18.1 `Transform` (only the members the
//! simulation uses: fields, IDENTITY/Default, from_matrix, from_translation,
//! with_rotation, to_matrix). Bodies are verbatim from bevy_transform/src/components/transform.rs.
//! `default()` mirrors bevy::prelude::default.
#pragma once
#include <tuple>

#include "s3/core/rs.hpp"
#include "s3/game/animation_pose/glam.hpp"

namespace s3::game::transform_shim {

using glam::Mat4;
using glam::Quat;
using glam::Vec3;

struct Transform {
    Vec3 translation{0.0f, 0.0f, 0.0f};
    Quat rotation = Quat::IDENTITY;
    Vec3 scale{1.0f, 1.0f, 1.0f};

    static const Transform IDENTITY;

    static Transform from_matrix(const Mat4& world_from_local) {
        auto [scale, rotation, translation] = world_from_local.to_scale_rotation_translation();
        return Transform{translation, rotation, scale};
    }
    static Transform from_translation(Vec3 translation) {
        Transform t{};
        t.translation = translation;
        return t;
    }
    [[nodiscard]] Transform with_rotation(Quat rotation) const {
        Transform t = *this;
        t.rotation = rotation;
        return t;
    }
    Mat4 to_matrix() const { return Mat4::from_scale_rotation_translation(scale, rotation, translation); }
};
inline const Transform Transform::IDENTITY = Transform{};
[[maybe_unused]] inline bool operator==(const Transform& a, const Transform& b) {
    return a.translation.x == b.translation.x && a.translation.y == b.translation.y &&
           a.translation.z == b.translation.z && a.rotation.x == b.rotation.x && a.rotation.y == b.rotation.y &&
           a.rotation.z == b.rotation.z && a.rotation.w == b.rotation.w && a.scale.x == b.scale.x &&
           a.scale.y == b.scale.y && a.scale.z == b.scale.z;
}
[[maybe_unused]] inline bool operator!=(const Transform& a, const Transform& b) { return !(a == b); }

/// bevy_utils::default
template <class T>
inline T default_() {
    return T{};
}

}  // namespace s3::game::transform_shim
